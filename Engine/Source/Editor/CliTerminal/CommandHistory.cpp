#if EDITOR

#include "CommandHistory.h"

void CommandHistory::Add(const std::string& cmd)
{
    if (cmd.empty())
    {
        return;
    }

    if (!mItems.empty() && mItems.back() == cmd)
    {
        ResetCursor();
        return;
    }

    mItems.push_back(cmd);
    while (mItems.size() > kMaxItems)
    {
        mItems.pop_front();
    }
    ResetCursor();
}

const std::string* CommandHistory::Prev()
{
    if (mItems.empty())
    {
        return nullptr;
    }

    if (mCursor == -1)
    {
        mCursor = static_cast<int>(mItems.size()) - 1;
    }
    else if (mCursor > 0)
    {
        --mCursor;
    }

    return &mItems[static_cast<size_t>(mCursor)];
}

const std::string* CommandHistory::Next()
{
    if (mItems.empty() || mCursor == -1)
    {
        return nullptr;
    }

    ++mCursor;
    if (mCursor >= static_cast<int>(mItems.size()))
    {
        mCursor = -1;
        return nullptr;
    }

    return &mItems[static_cast<size_t>(mCursor)];
}

void CommandHistory::SaveTo(rapidjson::Value& arr, rapidjson::Document::AllocatorType& alloc) const
{
    arr.SetArray();
    for (const std::string& s : mItems)
    {
        rapidjson::Value v;
        v.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
        arr.PushBack(v, alloc);
    }
}

void CommandHistory::LoadFrom(const rapidjson::Value& arr)
{
    mItems.clear();
    ResetCursor();

    if (!arr.IsArray())
    {
        return;
    }

    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
    {
        const rapidjson::Value& v = arr[i];
        if (v.IsString())
        {
            mItems.emplace_back(v.GetString(), v.GetStringLength());
        }
    }

    while (mItems.size() > kMaxItems)
    {
        mItems.pop_front();
    }
}

#endif
