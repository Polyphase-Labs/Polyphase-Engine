#include "Nodes/Widgets/ArrayWidget.h"
#include "Renderer.h"

#if EDITOR
#include "imgui.h"
#endif

FORCE_LINK_DEF(ArrayWidget);
DEFINE_NODE(ArrayWidget, Widget);

static const char* sArrayOrientationStrings[] =
{
    "Vertical",
    "Horizontal"
};
static_assert(int32_t(ArrayOrientation::Count) == 2, "Need to update string conversion table");


void ArrayWidget::Create()
{
    Super::Create();
    SetName("Array");
}

static bool sArrayRefreshLayoutStub = false;

void ArrayWidget::GatherProperties(std::vector<Property>& outProps)
{
    Widget::GatherProperties(outProps);

    SCOPED_CATEGORY("Array");

    outProps.push_back(Property(DatumType::Float, "Spacing", this, &mSpacing));
    outProps.push_back(Property(DatumType::Byte, "Orientation", this, &mOrientation, 1, nullptr, NULL_DATUM, int32_t(ArrayOrientation::Count), sArrayOrientationStrings));
    outProps.push_back(Property(DatumType::Bool, "Center", this, &mCenter));
    outProps.push_back(Property(DatumType::Float, "Padding Left", this, &mPaddingLeft));
    outProps.push_back(Property(DatumType::Float, "Padding Top", this, &mPaddingTop));
    outProps.push_back(Property(DatumType::Float, "Padding Right", this, &mPaddingRight));
    outProps.push_back(Property(DatumType::Float, "Padding Bottom", this, &mPaddingBottom));
    outProps.push_back(Property(DatumType::Bool, "Refresh Layout", this, &sArrayRefreshLayoutStub));
}

void ArrayWidget::UpdateRect()
{
    // Compute our own rect first so LayoutChildren can slice it into slots.
    Widget::UpdateRect();
    LayoutChildren();
}

void ArrayWidget::PreRender()
{
    // LayoutChildren must run every frame — the ArrayWidget itself often isn't
    // marked dirty when the layout changes (e.g. a child is added, removed, or
    // reordered; SetParent only marks the CHILD dirty, not the parent). When
    // ArrayWidget IS dirty, Widget::PreRender calls UpdateRect (virtual → our
    // override) which also calls LayoutChildren — the second call here is
    // idempotent, just re-stamping the same slot rects.
    Widget::PreRender();
    LayoutChildren();
}

#if EDITOR
bool ArrayWidget::DrawCustomProperty(Property& prop)
{
    if (prop.mName == "Refresh Layout")
    {
        if (ImGui::Button("Refresh Layout"))
        {
            // Belt-and-suspenders: dirty self, dirty every widget descendant
            // (so their PreRender re-runs UpdateRect), immediately re-run the
            // layout pass, then flag Renderer for a global widget dirty pass.
            MarkDirty();
            for (uint32_t i = 0; i < GetNumChildren(); ++i)
            {
                Widget* child = GetChildWidget(i);
                if (child) child->MarkDirty();
            }
            LayoutChildren();
            Renderer::Get()->DirtyAllWidgets();
        }
        return true;
    }
    return false;
}
#endif

void ArrayWidget::SetCentered(bool center)
{
    mCenter = center;
}

bool ArrayWidget::IsCentered() const
{
    return mCenter;
}

void ArrayWidget::SetSpacing(float spacing)
{
    mSpacing = spacing;
}

float ArrayWidget::GetSpacing() const
{
    return mSpacing;
}

void ArrayWidget::SetOrientation(ArrayOrientation orientation)
{
    mOrientation = orientation;
}

ArrayOrientation ArrayWidget::GetOrientation() const
{
    return mOrientation;
}

void ArrayWidget::LayoutChildren()
{
    const bool vertical = (mOrientation == ArrayOrientation::Vertical);
    const uint32_t numChildren = GetNumChildren();

    // Pass 1: total non-fill size along the array axis + count fill children.
    float nonFillTotal = 0.0f;
    uint32_t fillCount = 0;
    uint32_t widgetChildCount = 0;
    for (uint32_t i = 0; i < numChildren; ++i)
    {
        Widget* child = GetChildWidget(i);
        if (child == nullptr)
            continue;

        widgetChildCount++;
        // Only Fill* modes are treated as flex-grow. Stretch* children fall
        // through the normal path — they may over-consume the array (their
        // GetWidth returns parentWidth * mSize.x), but that's the classic
        // stretch semantic. Users wanting real flex-grow should use Fill /
        // FillHorizontal / FillVertical, which explicitly signal that intent
        // AND don't collapse to zero-size when the non-fill siblings already
        // exceed the array total.
        const bool fillsAxis = vertical ? child->FillsY() : child->FillsX();
        if (fillsAxis)
        {
            fillCount++;
        }
        else
        {
            nonFillTotal += vertical ? child->GetHeight() : child->GetWidth();
        }
    }

    const float totalSpacing = widgetChildCount > 1 ? mSpacing * float(widgetChildCount - 1) : 0.0f;

    // axisTotal must live in the same coordinate space as child->GetWidth()
    // (authored / mSize pixels). Deriving it from selfRect.mWidth ÷ mAbsoluteScale
    // gives a consistent value in all three render passes — Scene2D, Scene 3D
    // viewport (letterbox helper active), and GamePreview (scale = 1). Using
    // GetWidth()/GetHeight() directly breaks in Scene 3D because the letterbox
    // shortcut in GetParentWidth returns scaled letterbox pixels.
    const Rect selfRect = GetRect();
    const glm::vec2 arrayScale = GetAbsoluteScale();
    const float axisScale = vertical
        ? glm::max(arrayScale.y, 0.0001f)
        : glm::max(arrayScale.x, 0.0001f);
    const float axisTotal = (vertical ? selfRect.mHeight : selfRect.mWidth) / axisScale;

    // Padding insets the layout area from all four sides. Naming per array axis:
    //   axisPadBefore / axisPadAfter — leading and trailing insets on the array axis.
    //   crossPadBefore / crossPadAfter — insets on the perpendicular axis.
    const float axisPadBefore  = vertical ? mPaddingTop    : mPaddingLeft;
    const float axisPadAfter   = vertical ? mPaddingBottom : mPaddingRight;
    const float crossPadBefore = vertical ? mPaddingLeft   : mPaddingTop;
    const float crossPadAfter  = vertical ? mPaddingRight  : mPaddingBottom;

    float fillSlot = 0.0f;
    if (fillCount > 0)
    {
        fillSlot = axisTotal - axisPadBefore - axisPadAfter - nonFillTotal - totalSpacing;
        if (fillSlot < 0.0f)
            fillSlot = 0.0f;
        fillSlot /= float(fillCount);
    }

    // Pass 2: position each child, stamping a slot rect for Fill children so
    // their UpdateRect fills the flex-grow allocation instead of the full parent.
    // Offset starts at the leading padding so the first child renders inside
    // the padded area rather than flush against the ArrayWidget's edge.
    float offset = axisPadBefore;
    for (uint32_t i = 0; i < numChildren; ++i)
    {
        Widget* child = GetChildWidget(i);
        if (child == nullptr)
            continue;

        // Only Fill* modes are treated as flex-grow. Stretch* children fall
        // through the normal path — they may over-consume the array (their
        // GetWidth returns parentWidth * mSize.x), but that's the classic
        // stretch semantic. Users wanting real flex-grow should use Fill /
        // FillHorizontal / FillVertical, which explicitly signal that intent
        // AND don't collapse to zero-size when the non-fill siblings already
        // exceed the array total.
        const bool fillsAxis = vertical ? child->FillsY() : child->FillsX();
        const float childAxisSize = fillsAxis
            ? fillSlot
            : (vertical ? child->GetHeight() : child->GetWidth());

        if (fillsAxis)
        {
            // Slot rect in world/screen coords — matches how parent->GetRect()
            // would be seen by the child. Non-fill siblings render via
            // `anchorPos + mOffset * mAbsoluteScale`, i.e. their authored
            // offsets get scaled by the ArrayWidget's own mAbsoluteScale (which
            // includes the GamePreview letterbox scale in Scene2D). We must
            // scale the array-axis slot dimensions the same way, or the Fill
            // child ends up authored-sized while its neighbours are scaled →
            // visible overlap and the "column appears in the wrong place" bug.
            Rect slot = selfRect;
            if (vertical)
            {
                // Array axis (Y).
                slot.mY = selfRect.mY + offset * arrayScale.y;
                slot.mHeight = childAxisSize * arrayScale.y;
                // Cross axis (X) inset by cross padding.
                slot.mX = selfRect.mX + crossPadBefore * arrayScale.x;
                slot.mWidth = selfRect.mWidth - (crossPadBefore + crossPadAfter) * arrayScale.x;
            }
            else
            {
                // Array axis (X).
                slot.mX = selfRect.mX + offset * arrayScale.x;
                slot.mWidth = childAxisSize * arrayScale.x;
                // Cross axis (Y) inset by cross padding.
                slot.mY = selfRect.mY + crossPadBefore * arrayScale.y;
                slot.mHeight = selfRect.mHeight - (crossPadBefore + crossPadAfter) * arrayScale.y;
            }
            child->SetParentRectOverride(slot);
        }
        else
        {
            // Non-Fill children still use the ArrayWidget's rect as their parent,
            // so compensate the offset for their anchor ratio — a Right-anchored
            // widget's origin is at parent.right, not parent.left. Only applies
            // to non-stretched axes (stretch modes treat mOffset as a ratio).
            const glm::vec2 childAnchorRatio = child->GetAnchorRatio();

            // Cross-axis Fill/Stretch children (e.g. FillHorizontal in a vertical
            // array) run their own fill/stretch math against parentRect. Without
            // a slot override they'd fill the ArrayWidget's ENTIRE cross axis and
            // ignore Padding{Left,Right} (for vertical) or Padding{Top,Bottom}
            // (for horizontal). Stamp a slot rect that pads the cross axis but
            // keeps the array axis full, so array-axis anchor math still works
            // via the SetX/SetY calls below.
            const bool crossFillsOrStretches = vertical
                ? (child->FillsX() || child->StretchX())
                : (child->FillsY() || child->StretchY());
            if (crossFillsOrStretches)
            {
                Rect slot = selfRect;
                if (vertical)
                {
                    slot.mX = selfRect.mX + crossPadBefore * arrayScale.x;
                    slot.mWidth = selfRect.mWidth - (crossPadBefore + crossPadAfter) * arrayScale.x;
                }
                else
                {
                    slot.mY = selfRect.mY + crossPadBefore * arrayScale.y;
                    slot.mHeight = selfRect.mHeight - (crossPadBefore + crossPadAfter) * arrayScale.y;
                }
                child->SetParentRectOverride(slot);
            }

            if (vertical)
            {
                const float axisPos = offset - childAnchorRatio.y * axisTotal;
                if (!child->StretchY())
                    child->SetY(axisPos);
                else
                    child->SetY(offset);

                // Cross-axis (X). ArrayWidget always owns cross-axis
                // positioning for pixel-mode children; when Center is off,
                // snap to the padded top edge (anchor-compensated) instead
                // of leaving stale offsets from a previous Center=on pass.
                if (!child->StretchX() && !child->FillsX())
                {
                    const float crossWidth = GetWidth();
                    const float paddedWidth = crossWidth - crossPadBefore - crossPadAfter;
                    float crossPos;
                    if (mCenter)
                    {
                        const float crossSize = child->GetWidth();
                        crossPos = crossPadBefore + (paddedWidth - crossSize) * 0.5f
                                   - childAnchorRatio.x * crossWidth;
                    }
                    else
                    {
                        // Snap to padded left edge, compensating for anchor.
                        crossPos = crossPadBefore - childAnchorRatio.x * crossWidth;
                    }
                    child->SetX(crossPos);
                }
            }
            else
            {
                const float axisPos = offset - childAnchorRatio.x * axisTotal;
                if (!child->StretchX())
                    child->SetX(axisPos);
                else
                    child->SetX(offset);

                // Cross-axis (Y). See vertical branch above.
                if (!child->StretchY() && !child->FillsY())
                {
                    const float crossHeight = GetHeight();
                    const float paddedHeight = crossHeight - crossPadBefore - crossPadAfter;
                    float crossPos;
                    if (mCenter)
                    {
                        const float crossSize = child->GetHeight();
                        crossPos = crossPadBefore + (paddedHeight - crossSize) * 0.5f
                                   - childAnchorRatio.y * crossHeight;
                    }
                    else
                    {
                        // Snap to padded top edge, compensating for anchor.
                        crossPos = crossPadBefore - childAnchorRatio.y * crossHeight;
                    }
                    child->SetY(crossPos);
                }
            }
        }

        offset += childAxisSize + mSpacing;
    }
}
