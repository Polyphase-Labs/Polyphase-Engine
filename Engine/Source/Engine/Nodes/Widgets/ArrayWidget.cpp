#include "Nodes/Widgets/ArrayWidget.h"

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

void ArrayWidget::GatherProperties(std::vector<Property>& outProps)
{
    Widget::GatherProperties(outProps);

    SCOPED_CATEGORY("Array");

    outProps.push_back(Property(DatumType::Float, "Spacing", this, &mSpacing));
    outProps.push_back(Property(DatumType::Byte, "Orientation", this, &mOrientation, 1, nullptr, NULL_DATUM, int32_t(ArrayOrientation::Count), sArrayOrientationStrings));
    outProps.push_back(Property(DatumType::Bool, "Center", this, &mCenter));
}

void ArrayWidget::UpdateRect()
{
    // Compute our own rect first so LayoutChildren can slice it into slots.
    Widget::UpdateRect();
    LayoutChildren();
}

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
    const float axisTotal = vertical ? GetHeight() : GetWidth();
    float fillSlot = 0.0f;
    if (fillCount > 0)
    {
        fillSlot = axisTotal - nonFillTotal - totalSpacing;
        if (fillSlot < 0.0f)
            fillSlot = 0.0f;
        fillSlot /= float(fillCount);
    }

    // Pass 2: position each child, stamping a slot rect for Fill children so
    // their UpdateRect fills the flex-grow allocation instead of the full parent.
    const Rect selfRect = GetRect();
    float offset = 0.0f;
    for (uint32_t i = 0; i < numChildren; ++i)
    {
        Widget* child = GetChildWidget(i);
        if (child == nullptr)
            continue;

        const bool fillsAxis = vertical ? child->FillsY() : child->FillsX();
        const float childAxisSize = fillsAxis
            ? fillSlot
            : (vertical ? child->GetHeight() : child->GetWidth());

        if (fillsAxis)
        {
            // Slot rect in world/screen coords — matches how parent->GetRect()
            // would be seen by the child. For Fill children, the slot IS the
            // parent for layout purposes.
            Rect slot = selfRect;
            if (vertical)
            {
                slot.mY = selfRect.mY + offset;
                slot.mHeight = childAxisSize;
            }
            else
            {
                slot.mX = selfRect.mX + offset;
                slot.mWidth = childAxisSize;
            }
            child->SetParentRectOverride(slot);
        }

        if (vertical)
        {
            if (mCenter && !fillsAxis)
            {
                float x = GetWidth() / 2.0f - child->GetWidth() / 2.0f;
                child->SetX(x);
            }

            if (!fillsAxis)
                child->SetY(offset);
        }
        else
        {
            if (mCenter && !fillsAxis)
            {
                float y = GetHeight() / 2.0f - child->GetHeight() / 2.0f;
                child->SetY(y);
            }

            if (!fillsAxis)
                child->SetX(offset);
        }

        offset += childAxisSize + mSpacing;
    }
}
