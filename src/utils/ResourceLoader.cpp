#include "utils/ResourceLoader.h"

#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include <format>

namespace overlay::utils {

namespace {

std::string HResultToString(HRESULT hr) {
    return std::format("HRESULT=0x{:08X}", static_cast<uint32_t>(hr));
}

} // namespace

bool ResourceLoader::Initialize() {
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wic_factory_));

    if (FAILED(hr)) {
        LOG_ERROR(std::format("Failed to create WIC imaging factory: {}",
                              HResultToString(hr)));
        return false;
    }

    LOG_INFO("WIC imaging factory created.");
    return true;
}

std::optional<ImageData> ResourceLoader::LoadImage(const std::filesystem::path& path) {
    if (!wic_factory_) {
        LOG_ERROR("ResourceLoader not initialized.");
        return std::nullopt;
    }

    if (!std::filesystem::exists(path)) {
        LOG_ERROR(std::format("Image file not found: {}",
                              WstringToUtf8(path.wstring())));
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wic_factory_->CreateDecoderFromFilename(
        path.wstring().c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreateDecoderFromFilename failed for {}: {}",
                              WstringToUtf8(path.wstring()),
                              HResultToString(hr)));
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IWICBitmapDecoder::GetFrame failed: {}",
                              HResultToString(hr)));
        return std::nullopt;
    }

    ImageData data;
    hr = frame->GetSize(&data.width, &data.height);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IWICBitmapFrameDecode::GetSize failed: {}",
                              HResultToString(hr)));
        return std::nullopt;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wic_factory_->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreateFormatConverter failed: {}",
                              HResultToString(hr)));
        return std::nullopt;
    }

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IWICFormatConverter::Initialize failed: {}",
                              HResultToString(hr)));
        return std::nullopt;
    }

    // 32bppRGBA，每像素 4 字节，每行 4 字节对齐
    data.pitch = data.width * 4;
    const std::size_t buffer_size = static_cast<std::size_t>(data.pitch) * data.height;
    data.pixels.resize(buffer_size);
    data.pixel_format = GUID_WICPixelFormat32bppRGBA;

    hr = converter->CopyPixels(
        nullptr,
        data.pitch,
        static_cast<UINT>(buffer_size),
        data.pixels.data());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IWICFormatConverter::CopyPixels failed: {}",
                              HResultToString(hr)));
        return std::nullopt;
    }

    LOG_INFO(std::format("Loaded image {} ({}x{}, {} bytes)",
                         WstringToUtf8(path.wstring()),
                         data.width, data.height, buffer_size));
    return data;
}

std::filesystem::path ResourceLoader::ResolvePath(const std::filesystem::path& base_dir,
                                                  const std::filesystem::path& relative) {
    if (relative.empty()) {
        return {};
    }
    // 若 relative 本身已是绝对路径则直接返回
    if (relative.is_absolute()) {
        return relative;
    }
    return std::filesystem::absolute(base_dir / relative);
}

} // namespace overlay::utils
