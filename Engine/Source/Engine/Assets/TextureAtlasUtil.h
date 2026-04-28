#pragma once

#include "PolyphaseAPI.h"
#include "glm/glm.hpp"

#include <cstdint>

// Compute the [u0,v0]..[u1,v1] UV rectangle for a single cell in a regular grid
// atlas, in [0,1] texture space. The grid is described by:
//   cols, rows           — number of columns / rows in the atlas
//   cellW, cellH         — pixel size of one cell
//   marginX, marginY     — pixel margin around the entire grid (left/top edge)
//   spacingX, spacingY   — pixel spacing between adjacent cells
//   cellIndex            — row-major cell index (0 = top-left)
//   texW, texH           — texture pixel dimensions
//
// Returns false if cellIndex is out of range or any dimension is non-positive.
// outUV0/outUV1 are unmodified on failure.
//
// Used by TileSet (TileSet.cpp) and SpriteAnimation (atlas mode) so they share
// one source of truth for the math.
POLYPHASE_API bool ComputeAtlasCellUV(
    int32_t cols,
    int32_t rows,
    int32_t cellW,
    int32_t cellH,
    int32_t marginX,
    int32_t marginY,
    int32_t spacingX,
    int32_t spacingY,
    int32_t cellIndex,
    int32_t texW,
    int32_t texH,
    glm::vec2& outUV0,
    glm::vec2& outUV1);
