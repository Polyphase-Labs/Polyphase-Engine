#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>

#include "PolyphaseAPI.h"

struct lua_State;

namespace rapidjson
{
    // Forward decls (consumer .cpp files include the full rapidjson headers).
    template<typename Encoding, typename Allocator> class GenericValue;
    template<typename Encoding, typename Allocator, typename StackAllocator> class GenericDocument;
    template<typename CharType> struct UTF8;
    class CrtAllocator;
    template<typename BaseAllocator> class MemoryPoolAllocator;
    using Document = GenericDocument<UTF8<char>, MemoryPoolAllocator<CrtAllocator>, CrtAllocator>;
    using Value    = GenericValue<UTF8<char>, MemoryPoolAllocator<CrtAllocator>>;
}

// Parse a UTF-8 byte buffer as JSON. On success returns true and `outDoc` is
// populated. On failure returns false; `outError` carries the parser message.
POLYPHASE_API bool ParseJsonBytes(const uint8_t* data, size_t size,
                                  rapidjson::Document& outDoc,
                                  std::string& outError);

// Push a rapidjson value onto the Lua stack. Recursive: object → table,
// array → integer-indexed table, primitives → primitives.
POLYPHASE_API void RapidJsonValueToLua(lua_State* L, const rapidjson::Value& v);

// Pop a Lua value at index `idx` and convert it to a rapidjson value.
// `alloc` must be the document's allocator. Returns true on success.
POLYPHASE_API bool LuaToRapidJsonValue(lua_State* L, int idx,
                                       rapidjson::Value& outVal,
                                       rapidjson::Document& doc);
