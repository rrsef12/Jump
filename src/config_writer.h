#pragma once
#include "model.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace jump {
// A single writer owns persistence ordering. Automatic records coalesce, while
// explicit saves wait for their snapshot; an older write can never finish last.
class ConfigWriter {
  public:
    using Write = std::function<bool(const Config &, const std::filesystem::path &)>;

  private:
    struct Request {
        Config config;
        ULONG_PTR revision = 0;
        bool immediate = false;
        std::chrono::steady_clock::time_point due;
    };
    std::filesystem::path file;
    HWND window;
    UINT message;
    Write write;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::optional<Request> pending;
    ULONG_PTR latest = 0, completed = 0;
    bool stopping = false, lastSuccess = true;
    std::thread worker;
    void run() {
        std::unique_lock lock(mutex);
        for (;;) {
            changed.wait(lock, [&] { return stopping || pending.has_value(); });
            if (!pending) {
                if (stopping)
                    return;
                continue;
            }
            if (!stopping && !pending->immediate) {
                auto revision = pending->revision;
                auto due = pending->due;
                if (changed.wait_until(lock, due, [&] {
                        return stopping || !pending || pending->revision != revision || pending->immediate;
                    }))
                    continue;
            }
            auto request = std::move(*pending);
            pending.reset();
            lock.unlock();
            bool success = false;
            try {
                success = write(request.config, file);
            } catch (...) {
                success = false;
            }
            lock.lock();
            completed = request.revision;
            lastSuccess = success;
            if (window)
                PostMessageW(window, message, completed, success);
            changed.notify_all();
        }
    }

  public:
    ConfigWriter(std::filesystem::path path, HWND target, UINT notification, Write output = {})
        : file(std::move(path)), window(target), message(notification),
          write(output ? std::move(output) : Write(save)), worker([this] { run(); }) {}
    ~ConfigWriter() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        worker.join();
    }
    ULONG_PTR request(const Config &config, bool immediate = false) {
        std::lock_guard lock(mutex);
        pending = Request{config, ++latest, immediate,
                          std::chrono::steady_clock::now() + std::chrono::milliseconds(180)};
        changed.notify_all();
        return latest;
    }
    bool saveNow(const Config &config) {
        auto revision = request(config, true);
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return completed >= revision; });
        return lastSuccess;
    }
    bool flush() {
        std::unique_lock lock(mutex);
        auto revision = latest;
        if (pending)
            pending->immediate = true;
        changed.notify_all();
        changed.wait(lock, [&] { return completed >= revision; });
        return lastSuccess;
    }
    ULONG_PTR revision() const {
        std::lock_guard lock(mutex);
        return latest;
    }
};
} // namespace jump
