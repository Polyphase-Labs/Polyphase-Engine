#include "Assets/TextureAtlasUtil.h"

bool ComputeAtlasCellUV(
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
    glm::vec2& outUV1)
{
    if (cellIndex < 0 || cols <= 0 || rows <= 0 || cellIndex >= cols * rows)
        return false;

    if (cellW <= 0 || cellH <= 0 || texW <= 0 || texH <= 0)
        return false;

    const int32_t col = cellIndex % cols;
    const int32_t row = cellIndex / cols;

    const float fTexW = static_cast<float>(texW);
    const float fTexH = static_cast<float>(texH);

    const float px0 = static_cast<float>(marginX + col * (cellW + spacingX));
    const float py0 = static_cast<float>(marginY + row * (cellH + spacingY));
    const float px1 = px0 + static_cast<float>(cellW);
    const float py1 = py0 + static_cast<float>(cellH);

    outUV0 = { px0 / fTexW, py0 / fTexH };
    outUV1 = { px1 / fTexW, py1 / fTexH };
    return true;
}
