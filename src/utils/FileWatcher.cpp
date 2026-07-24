#include "utils/FileWatcher.h"

#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include <format>

namespace overlay::utils {

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    Stop();
}

bool FileWatcher::Start(const std::filesystem::path& file_path,
                        HWND notify_hwnd, UINT msg_id) {
    if (running_.load()) return false;

    watch_dir_ = file_path.parent_path();
    filter_name_ = file_path.filename().wstring();
    notify_hwnd_ = notify_hwnd;
    msg_id_ = msg_id;

    if (watch_dir_.empty() || filter_name_.empty()) {
        LOG_ERROR("FileWatcher: invalid file path.");
        return false;
    }

    hDir_ = CreateFileW(
        watch_dir_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (hDir_ == INVALID_HANDLE_VALUE) {
        LOG_ERROR(std::format("FileWatcher: failed to open directory {}.",
                              watch_dir_.string()));
        return false;
    }

    running_.store(true);
    paused_.store(false);
    changed_.store(false);

    hThread_ = CreateThread(nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* self = static_cast<FileWatcher*>(param);
            self->WorkerThread();
            return 0;
        },
        this, 0, nullptr);

    if (!hThread_) {
        running_.store(false);
        CloseHandle(hDir_);
        hDir_ = INVALID_HANDLE_VALUE;
        LOG_ERROR("FileWatcher: failed to create worker thread.");
        return false;
    }

    LOG_INFO(std::format("FileWatcher: watching {} for changes to {}",
                         watch_dir_.string(),
                         overlay::utils::WstringToUtf8(filter_name_)));
    return true;
}

void FileWatcher::Stop() {
    if (!running_.load()) return;

    running_.store(false);

    if (hDir_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(hDir_, nullptr);
    }

    if (hThread_) {
        WaitForSingleObject(hThread_, 2000);
        CloseHandle(hThread_);
        hThread_ = nullptr;
    }

    if (hDir_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hDir_);
        hDir_ = INVALID_HANDLE_VALUE;
    }

    notify_hwnd_ = nullptr;
    msg_id_ = 0;
    changed_.store(false);
}

bool FileWatcher::Poll() {
    if (!changed_.load(std::memory_order_acquire)) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_change_time_).count();

    // Debounce: ignore changes within 300 ms of the previous one.
    if (elapsed < 300) return false;

    changed_.store(false, std::memory_order_release);
    last_change_time_ = now;
    return true;
}

void FileWatcher::WorkerThread() {
    alignas(DWORD) BYTE buffer[4096];
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (!overlapped.hEvent) {
        LOG_ERROR("FileWatcher: failed to create overlapped event.");
        return;
    }

    while (running_.load(std::memory_order_acquire)) {
        DWORD bytes_returned = 0;
        BOOL result = ReadDirectoryChangesW(
            hDir_,
            buffer,
            sizeof(buffer),
            FALSE, // non-recursive
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &bytes_returned,
            &overlapped,
            nullptr);

        if (!result) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED || !running_.load()) {
                break;
            }
            LOG_ERROR(std::format("FileWatcher: ReadDirectoryChangesW failed: {}", err));
            break;
        }

        // Wait for the overlapped operation or shutdown signal.
        HANDLE waits[1] = {overlapped.hEvent};
        DWORD wait_result = WaitForMultipleObjects(1, waits, FALSE, 500);

        if (wait_result == WAIT_TIMEOUT) {
            continue;
        }

        if (wait_result == WAIT_OBJECT_0) {
            if (!GetOverlappedResult(hDir_, &overlapped, &bytes_returned, FALSE)) {
                DWORD err = GetLastError();
                if (err == ERROR_OPERATION_ABORTED || !running_.load()) {
                    break;
                }
                continue;
            }

            if (bytes_returned == 0) {
                ResetEvent(overlapped.hEvent);
                continue;
            }

            bool match = false;
            FILE_NOTIFY_INFORMATION* info =
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            while (true) {
                std::wstring name(info->FileName,
                                  info->FileNameLength / sizeof(WCHAR));
                if (name == filter_name_) {
                    if (info->Action == FILE_ACTION_MODIFIED ||
                        info->Action == FILE_ACTION_ADDED ||
                        info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                        match = true;
                        break;
                    }
                }
                if (info->NextEntryOffset == 0) break;
                info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
            }

            ResetEvent(overlapped.hEvent);

            if (match && !paused_.load(std::memory_order_acquire)) {
                changed_.store(true, std::memory_order_release);
                if (notify_hwnd_ && msg_id_) {
                    PostMessageW(notify_hwnd_, msg_id_, 0, 0);
                }
            }
        }
    }

    CloseHandle(overlapped.hEvent);
}

} // namespace overlay::utils
