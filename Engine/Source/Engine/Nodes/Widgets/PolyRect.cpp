#include "Nodes/Widgets/PolyRect.h"

FORCE_LINK_DEF(PolyRect);
DEFINE_NODE(PolyRect, Poly);

PolyRect::PolyRect()
{

}

PolyRect::~PolyRect()
{

}

void PolyRect::UpdateGeometry()
{
    if (IsDirty())
    {
        float width = mRect.mWidth;
        float height = mRect.mHeight;

        ClearVertices();
        const glm::vec4 color = GetColor();
        AddVertex({ 0.0f, 0.0f }, color);
        AddVertex({ 0.0f, height }, color);
        AddVertex({ width, height }, color);
        AddVertex({ width, 0.0f }, color);
        AddVertex({ 0.0f, 0.0f }, color);
    }
}
