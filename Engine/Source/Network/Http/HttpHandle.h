#pragma once

#include <atomic>
#include <memory>
#include <stdint.h>

#include "PolyphaseAPI.h"

class POLYPHASE_API HttpHandle
{
public:
    HttpHandle() = default;
    HttpHandle(uint64_t id, std::shared_ptr<std::atomic<bool>> cancelFlag)
        : mId(id), mCancelFlag(std::move(cancelFlag)) {}

    uint64_t GetId() const { return mId; }
    bool     IsValid() const { return mCancelFlag != nullptr; }

    // Marks the request for cancellation. The worker thread checks the flag
    // during its loop and aborts cleanly. Idempotent and safe to call from any
    // thread; safe to call after the request has already completed.
    void Cancel();
    bool IsCancelled() const;

private:
    uint64_t mId = 0;
    std::shared_ptr<std::atomic<bool>> mCancelFlag;
};
