#pragma once
#include "naming_worker.h"

namespace jump {
struct SaveTargetRequest {unsigned long long id=0,generation=0;std::wstring path;};
struct SaveTargetInfo {
    SaveTargetRequest request;
    bool exists=false,folder=false;
    DWORD error=0;
    unsigned long long bytes=0;
    FILETIME modified{};
};
inline SaveTargetInfo inspectSaveTarget(const SaveTargetRequest &request){
    SaveTargetInfo info;info.request=request;WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if(!GetFileAttributesExW(request.path.c_str(),GetFileExInfoStandard,&attributes)){
        auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)info.error=error;
        return info;
    }
    info.exists=true;info.folder=(attributes.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
    info.bytes=(static_cast<unsigned long long>(attributes.nFileSizeHigh)<<32)|attributes.nFileSizeLow;
    info.modified=attributes.ftLastWriteTime;return info;
}
inline std::wstring saveTargetDetails(const SaveTargetInfo &info){
    if(info.error)return L"无法读取目标文件信息 · 点击重试";
    FILETIME local{};SYSTEMTIME time{};FileTimeToLocalFileTime(&info.modified,&local);FileTimeToSystemTime(&local,&time);
    wchar_t stamp[40]{};swprintf_s(stamp,L"%04d-%02d-%02d %02d:%02d",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute);
    if(info.folder)return std::wstring(stamp)+L" · 文件夹";
    wchar_t size[40]{};double bytes=static_cast<double>(info.bytes);
    if(bytes>=1024*1024*1024)swprintf_s(size,L"%.1f GB",bytes/(1024*1024*1024));
    else if(bytes>=1024*1024)swprintf_s(size,L"%.1f MB",bytes/(1024*1024));
    else if(bytes>=1024)swprintf_s(size,L"%.1f KB",bytes/1024);
    else swprintf_s(size,L"%llu B",info.bytes);
    return std::wstring(stamp)+L" · "+size;
}
// Two bounded workers keep a slow network path from blocking a newer target.
// Only exact-path metadata is read; no directory scans, file reads or idle polling.
class SaveTargetWorker {
    struct Shared {
        std::mutex mutex;std::condition_variable ready;
        std::optional<SaveTargetRequest> pending;std::optional<SaveTargetInfo> result;
        unsigned long long latest=0;bool stop=false;HWND window{};UINT message{};
    };
    std::shared_ptr<Shared> state;
public:
    SaveTargetWorker(HWND window,UINT message):state(std::make_shared<Shared>()){
        state->window=window;state->message=message;
        for(int i=0;i<2;++i)std::thread([s=state]{for(;;){SaveTargetRequest request;
            {std::unique_lock lock(s->mutex);s->ready.wait(lock,[&]{return s->stop||s->pending.has_value();});
                if(s->stop)return;request=std::move(*s->pending);s->pending.reset();}
            auto result=inspectSaveTarget(request);
            std::lock_guard lock(s->mutex);if(s->stop||request.id!=s->latest)continue;
            s->result=std::move(result);PostMessageW(s->window,s->message,0,0);
        }}).detach();
    }
    ~SaveTargetWorker(){{std::lock_guard lock(state->mutex);state->stop=true;state->pending.reset();state->result.reset();}state->ready.notify_all();}
    void cancel(){std::lock_guard lock(state->mutex);++state->latest;state->pending.reset();state->result.reset();}
    void request(SaveTargetRequest request){std::lock_guard lock(state->mutex);request.id=++state->latest;state->pending=std::move(request);state->result.reset();state->ready.notify_one();}
    std::optional<SaveTargetInfo> take(){std::lock_guard lock(state->mutex);auto result=std::move(state->result);state->result.reset();return result;}
};
} // namespace jump
