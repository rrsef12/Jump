#pragma once
#include "model.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace jump {
struct NamingRequest {
    unsigned long long id = 0, generation = 0;
    std::wstring key, source, directory, extension, displayed;
    SmartNaming options;
    bool submit = false;
};
struct NamingResult {
    NamingRequest request;
    RenamePreview preview;
};

// No UI or adapter state is accessed by these workers. Only the newest request
// is retained; two bounded workers allow a new directory to bypass a slow one.
class NamingWorker {
  public:
    using Calculate = std::function<RenamePreview(const NamingRequest &, const std::function<bool()> &)>;

  private:
    struct Shared {
        std::mutex mutex;
        std::condition_variable ready;
        std::optional<NamingRequest> pending;
        std::deque<NamingResult> results;
        std::atomic<unsigned long long> latest{0};
        std::atomic<bool> stop{false};
        HWND window{};
        UINT message{};
        Calculate calculate;
    };
    std::shared_ptr<Shared> state;

  public:
    NamingWorker(HWND window, UINT message, Calculate calculate = {}) : state(std::make_shared<Shared>()) {
        state->window = window;
        state->message = message;
        state->calculate = calculate ? std::move(calculate)
                                     : [](const NamingRequest &r, const std::function<bool()> &cancelled) {
                                           return smartNamingPreview(r.source, r.directory, r.extension,
                                                                     r.options, cancelled);
                                       };
        for (int i = 0; i < 2; ++i)
            std::thread([s = state] {
                for (;;) {
                    NamingRequest request;
                    {
                        std::unique_lock lock(s->mutex);
                        s->ready.wait(lock, [&] { return s->stop || s->pending.has_value(); });
                        if (s->stop)
                            return;
                        request = std::move(*s->pending);
                        s->pending.reset();
                    }
                    auto cancelled = [&] { return s->stop || s->latest != request.id; };
                    RenamePreview preview;
                    try {
                        preview = s->calculate(request, cancelled);
                    } catch (...) {
                        preview.error = L"编号计算失败，请调整名称或目录后重试。";
                    }
                    std::lock_guard lock(s->mutex);
                    if (cancelled())
                        continue;
                    s->results.clear();
                    s->results.push_back({std::move(request), std::move(preview)});
                    PostMessageW(s->window, s->message, 0, 0);
                }
            }).detach();
    }
    ~NamingWorker() {
        {
            std::lock_guard lock(state->mutex);
            state->stop = true;
            state->pending.reset();
            state->results.clear();
        }
        state->ready.notify_all();
    }
    unsigned long long cancel() {
        std::lock_guard lock(state->mutex);
        state->pending.reset();
        state->results.clear();
        return ++state->latest;
    }
    unsigned long long request(NamingRequest request) {
        std::lock_guard lock(state->mutex);
        request.id = ++state->latest;
        state->pending = std::move(request);
        state->results.clear();
        state->ready.notify_one();
        return state->latest;
    }
    std::optional<NamingResult> take() {
        std::lock_guard lock(state->mutex);
        if (state->results.empty())
            return {};
        auto result = std::move(state->results.front());
        state->results.pop_front();
        return result;
    }
};
} // namespace jump
