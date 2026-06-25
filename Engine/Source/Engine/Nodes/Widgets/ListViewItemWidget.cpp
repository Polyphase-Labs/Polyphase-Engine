#include "Nodes/Widgets/ListViewItemWidget.h"
#include "Nodes/Widgets/ListViewWidget.h"
#include "InputDevices.h"

FORCE_LINK_DEF(ListViewItemWidget);
DEFINE_NODE(ListViewItemWidget, Widget);

void ListViewItemWidget::Create()
{
    Widget::Create();
    SetName("ListViewItem");

    // Default size - will be overridden by content or ListView settings
    SetDimensions(100.0f, 30.0f);
}

void ListViewItemWidget::Tick(float deltaTime)
{
    Widget::Tick(deltaTime);

    HandlePointerEvents();
}

void ListViewItemWidget::SetIndex(int32_t index)
{
    mIndex = index;
}

int32_t ListViewItemWidget::GetIndex() const
{
    return mIndex;
}

void ListViewItemWidget::SetListView(ListViewWidget* listView)
{
    mListView = listView;
}

ListViewWidget* ListViewItemWidget::GetListView() const
{
    return mListView;
}

void ListViewItemWidget::SetContentWidget(Widget* widget)
{
    mContentWidget = widget;

    // Auto-size to content if no fixed size
    if (mContentWidget != nullptr)
    {
        // Check if parent ListView has fixed sizes
        bool hasFixedWidth = false;
        bool hasFixedHeight = false;
        if (mListView != nullptr)
        {
            hasFixedWidth = mListView->GetItemWidth() > 0.0f;
            hasFixedHeight = mListView->GetItemHeight() > 0.0f;
        }

        if (!hasFixedWidth)
        {
            SetWidth(mContentWidget->GetWidth());
        }
        if (!hasFixedHeight)
        {
            SetHeight(mContentWidget->GetHeight());
        }
    }
}

Widget* ListViewItemWidget::GetContentWidget() const
{
    return mContentWidget;
}

void ListViewItemWidget::SetSelected(bool selected)
{
    if (mSelected != selected)
    {
        mSelected = selected;

        if (mSelected)
        {
            EmitSignal("Selected", { this });
            CallFunction("OnSelected", { this });
        }
        else
        {
            EmitSignal("Deselected", { this });
            CallFunction("OnDeselected", { this });
        }
    }
}

bool ListViewItemWidget::IsSelected() const
{
    return mSelected;
}

bool ListViewItemWidget::IsHovered() const
{
    return mHovered;
}

void ListViewItemWidget::HandlePointerEvents()
{
    if (!ShouldHandleInput() || mListView == nullptr)
    {
        return;
    }

    bool isHovered = ContainsMouse();

    // The ListView callbacks below fire user Lua (OnItemClicked / OnItem*),
    // which routinely rebuilds the list (e.g. directory navigation in a file
    // browser) -- removing every ListViewItemWidget from its parent and
    // dropping the SharedPtr that kept `this` alive. Snapshot a WeakPtr to
    // self before each ListView call so we can detect the destruction and
    // bail before EmitSignal / CallFunction / `mWasHovered = ...` dereferences
    // freed memory or hits the lua_isuserdata assert in CallMethod.
    NodePtrWeak selfWeak = GetSelfPtr();

    // Hover enter
    if (isHovered && !mWasHovered)
    {
        mHovered = true;
        mListView->OnItemHoverEnter(mIndex);
        if (!selfWeak.IsValid() || selfWeak->IsDestroyed())
            return;
        EmitSignal("HoverEnter", { this });
        CallFunction("OnHoverEnter", { this });
        if (!selfWeak.IsValid() || selfWeak->IsDestroyed())
            return;
    }
    // Hover exit
    else if (!isHovered && mWasHovered)
    {
        mHovered = false;
        mListView->OnItemHoverExit(mIndex);
        if (!selfWeak.IsValid() || selfWeak->IsDestroyed())
            return;
        EmitSignal("HoverExit", { this });
        CallFunction("OnHoverExit", { this });
        if (!selfWeak.IsValid() || selfWeak->IsDestroyed())
            return;
    }

    // Click - on pointer release while hovering
    if (isHovered && IsPointerJustUp(0))
    {
        mListView->OnItemClicked(mIndex);
        if (!selfWeak.IsValid() || selfWeak->IsDestroyed())
            return;
        EmitSignal("Clicked", { this });
        CallFunction("OnClicked", { this });
        if (!selfWeak.IsValid() || selfWeak->IsDestroyed())
            return;
    }

    mWasHovered = isHovered;
}
