#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <wincodec.h>
#include <wrl/client.h>

namespace overlay::utils {

// CPU 侧图片解码结果。像素格式通常为 GUID_WICPixelFormat32bppRGBA，
// 按行主序、自上而下排列；pitch 为每行字节数（已 4 字节对齐）。
struct ImageData {
    UINT width = 0;
    UINT height = 0;
    UINT pitch = 0;
    GUID pixel_format = GUID_NULL;
    std::vector<BYTE> pixels;
};

class ResourceLoader {
public:
    ResourceLoader() = default;
    ~ResourceLoader() = default;

    ResourceLoader(const ResourceLoader&) = delete;
    ResourceLoader& operator=(const ResourceLoader&) = delete;

    // 创建 WIC 成像工厂。调用方需先初始化 COM（CoInitializeEx）。
    bool Initialize();

    // 在 CoUninitialize 之前主动释放 WIC 工厂，避免 COM 已卸载后释放对象崩溃。
    void Shutdown() { wic_factory_.Reset(); }

    // 从绝对路径加载图片（PNG/JPG/BMP 等 WIC 支持的格式）。
    // 失败时返回 std::nullopt 并记录日志。
    std::optional<ImageData> LoadImage(const std::filesystem::path& path);

    // 按 exe 目录解析相对路径。relative 为空时返回空路径。
    static std::filesystem::path ResolvePath(const std::filesystem::path& base_dir,
                                             const std::filesystem::path& relative);

private:
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory_;
};

} // namespace overlay::utils
