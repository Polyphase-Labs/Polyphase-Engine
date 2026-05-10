#include "Network/Http/HttpHandle.h"

void HttpHandle::Cancel()
{
    if (mCancelFlag != nullptr)
    {
        mCancelFlag->store(true, std::memory_order_release);
    }
}

bool HttpHandle::IsCancelled() const
{
    return mCancelFlag != nullptr && mCancelFlag->load(std::memory_order_acquire);
}
