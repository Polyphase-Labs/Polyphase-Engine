#pragma once

// POLYPHASE_ENGINE_ABI: bump on any binary-incompatible change to the engine's
// exported types, vtables, function signatures, or struct layouts that cross
// the DLL/SO boundary when the engine is built as a shared runtime.
//
// This is intentionally distinct from:
//   - POLYPHASE_VERSION              (advances on feature / fix releases)
//   - POLYPHASE_PLUGIN_API_VERSION   (gates the C ABI for native addons)
//
// The runtime validator compares this value against the abi field embedded
// in <Project>/Vendor/PolyphaseRuntime/.../engine-runtime.json.
#define POLYPHASE_ENGINE_ABI 6
