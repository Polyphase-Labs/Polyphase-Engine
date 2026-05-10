#include "Network/Http/HttpClient.h"

#include "Network/Http/Backends/HttpBackend.h"
#include "Log.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace
{
    struct PendingRequest
    {
        uint64_t                              id = 0;
        HttpRequest                           request;
        HttpResponseCallback                  callback;
        std::shared_ptr<std::atomic<bool>>    cancelFlag;
    };

    struct CompletedRequest
    {
        uint64_t                              id = 0;
        HttpResponseCallback                  callback;
        HttpResponse                          response;
    };

    struct ClientState
    {
        std::unique_ptr<HttpBackend>      backend;

        std::mutex                        queueMutex;
        std::condition_variable           queueCv;
        std::deque<PendingRequest>        queue;
        std::shared_ptr<std::atomic<bool>> activeCancel;       // set while a request is running

        std::mutex                        completedMutex;
        std::deque<CompletedRequest>      completed;

        std::thread                       worker;
        std::atomic<bool>                 running{ false };
        // 32-bit ID counter — devkitPPC (PowerPC 32) has no native 8-byte
        // atomics and doesn't ship libatomic, so std::atomic<uint64_t>::fetch_add
        // would fail to link. 4 billion request IDs per session is more than
        // enough; on the rare wraparound the counter just rolls over.
        std::atomic<uint32_t>             nextId{ 1 };
    };

    ClientState* sState = nullptr;

    void WorkerLoop()
    {
        while (sState != nullptr && sState->running.load(std::memory_order_acquire))
        {
            PendingRequest pending;
            bool gotOne = false;
            {
                std::unique_lock<std::mutex> lock(sState->queueMutex);
                sState->queueCv.wait(lock, [] {
                    return !sState->running.load(std::memory_order_acquire)
                        || !sState->queue.empty();
                });

                if (!sState->running.load(std::memory_order_acquire))
                {
                    break;
                }

                if (!sState->queue.empty())
                {
                    pending = std::move(sState->queue.front());
                    sState->queue.pop_front();
                    sState->activeCancel = pending.cancelFlag;
                    gotOne = true;
                }
            }

            if (!gotOne)
            {
                continue;
            }

            HttpResponse response;

            if (pending.cancelFlag != nullptr
                && pending.cancelFlag->load(std::memory_order_acquire))
            {
                response.SetError(HttpError::Cancelled, "Cancelled before send");
            }
            else if (sState->backend == nullptr || !sState->backend->IsAvailable())
            {
                response.SetError(HttpError::Unavailable,
                    sState->backend != nullptr ? sState->backend->GetMissingDependencyMessage()
                                               : "HTTP backend not initialized");
            }
            else
            {
                std::atomic<bool>& flag = *pending.cancelFlag;
                sState->backend->PerformRequest(pending.request, flag, response);
            }

            {
                std::lock_guard<std::mutex> lock(sState->completedMutex);
                CompletedRequest done;
                done.id       = pending.id;
                done.callback = std::move(pending.callback);
                done.response = std::move(response);
                sState->completed.emplace_back(std::move(done));
            }

            {
                std::lock_guard<std::mutex> lock(sState->queueMutex);
                sState->activeCancel.reset();
            }
        }
    }
}

namespace Http
{
    void Initialize()
    {
        if (sState != nullptr)
        {
            return;
        }

        sState = new ClientState();
        sState->backend = CreatePlatformHttpBackend();

        if (sState->backend != nullptr)
        {
            const bool ok = sState->backend->Initialize();
            if (!ok)
            {
                LogWarning("HTTP backend Initialize() returned false: %s",
                    sState->backend->GetMissingDependencyMessage());
            }
        }

        sState->running.store(true, std::memory_order_release);
        sState->worker = std::thread(WorkerLoop);
    }

    void Shutdown()
    {
        if (sState == nullptr)
        {
            return;
        }

        sState->running.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(sState->queueMutex);
            for (auto& p : sState->queue)
            {
                if (p.cancelFlag != nullptr)
                {
                    p.cancelFlag->store(true, std::memory_order_release);
                }
            }
            if (sState->activeCancel != nullptr)
            {
                sState->activeCancel->store(true, std::memory_order_release);
            }
        }
        sState->queueCv.notify_all();

        if (sState->worker.joinable())
        {
            sState->worker.join();
        }

        // Drain the completion queue once so callbacks waiting on results can
        // observe a Cancelled response. Callers that registered after Shutdown
        // simply leak their callback (logged below).
        Tick();

        size_t leaked = 0;
        {
            std::lock_guard<std::mutex> lock(sState->completedMutex);
            leaked = sState->completed.size();
            sState->completed.clear();
        }
        if (leaked != 0)
        {
            LogWarning("Http::Shutdown discarded %zu pending completions", leaked);
        }

        if (sState->backend != nullptr)
        {
            sState->backend->Shutdown();
        }

        delete sState;
        sState = nullptr;
    }

    void Tick()
    {
        if (sState == nullptr)
        {
            return;
        }

        std::deque<CompletedRequest> drained;
        {
            std::lock_guard<std::mutex> lock(sState->completedMutex);
            drained.swap(sState->completed);
        }

        for (auto& done : drained)
        {
            if (done.callback)
            {
                done.callback(done.response);
            }
        }
    }

    bool IsAvailable()
    {
        return sState != nullptr
            && sState->backend != nullptr
            && sState->backend->IsAvailable();
    }

    const char* GetMissingDependencyMessage()
    {
        if (sState == nullptr || sState->backend == nullptr) return "Not initialized";
        return sState->backend->GetMissingDependencyMessage();
    }

    HttpHandle Send(HttpRequest req, HttpResponseCallback cb)
    {
        if (sState == nullptr)
        {
            // Return an immediately-completing error response on the next Tick.
            HttpResponse r;
            r.SetError(HttpError::NotInitialized, "Http::Initialize() was not called");
            HttpResponse copy = r;
            std::shared_ptr<std::atomic<bool>> flag = std::make_shared<std::atomic<bool>>(false);
            // No queue exists; just call back synchronously.
            if (cb) cb(copy);
            return HttpHandle(0, flag);
        }

        std::shared_ptr<std::atomic<bool>> flag = std::make_shared<std::atomic<bool>>(false);
        const uint64_t id = sState->nextId.fetch_add(1, std::memory_order_relaxed);

        PendingRequest p;
        p.id         = id;
        p.request    = std::move(req);
        p.callback   = std::move(cb);
        p.cancelFlag = flag;

        {
            std::lock_guard<std::mutex> lock(sState->queueMutex);
            sState->queue.emplace_back(std::move(p));
        }
        sState->queueCv.notify_one();

        return HttpHandle(id, flag);
    }

    HttpHandle Get(const std::string& url, HttpResponseCallback cb)
    {
        return Send(HttpRequest(HttpVerb::Get, url), std::move(cb));
    }

    HttpHandle Post(const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb)
    {
        return Send(HttpRequest(HttpVerb::Post, url).Body(std::move(body)), std::move(cb));
    }

    HttpHandle Put(const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb)
    {
        return Send(HttpRequest(HttpVerb::Put, url).Body(std::move(body)), std::move(cb));
    }

    HttpHandle Patch(const std::string& url, std::vector<uint8_t> body, HttpResponseCallback cb)
    {
        return Send(HttpRequest(HttpVerb::Patch, url).Body(std::move(body)), std::move(cb));
    }

    HttpHandle Delete(const std::string& url, HttpResponseCallback cb)
    {
        return Send(HttpRequest(HttpVerb::Delete, url), std::move(cb));
    }

    HttpHandle PostString(const std::string& url, const std::string& body, HttpResponseCallback cb)
    {
        return Send(HttpRequest(HttpVerb::Post, url).Body(body), std::move(cb));
    }

    HttpResponse SendSync(HttpRequest req)
    {
        HttpResponse response;
        if (sState == nullptr || sState->backend == nullptr || !sState->backend->IsAvailable())
        {
            response.SetError(HttpError::Unavailable,
                sState != nullptr && sState->backend != nullptr
                    ? sState->backend->GetMissingDependencyMessage()
                    : "HTTP not initialized");
            return response;
        }

        std::atomic<bool> cancel{ false };
        sState->backend->PerformRequest(req, cancel, response);
        return response;
    }
}
