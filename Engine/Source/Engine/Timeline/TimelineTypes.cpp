#include "Timeline/TimelineTypes.h"

#include "Nodes/Node.h"
#include "Nodes/3D/Node3d.h"
#include "Nodes/Widgets/Widget.h"

void ApplyTransformKeyframeToNode(Node* target, const TransformKeyframe& kf)
{
    if (target == nullptr)
    {
        return;
    }

    if (target->IsNode3D())
    {
        Node3D* node3d = static_cast<Node3D*>(target);
        node3d->SetPosition(kf.mPosition);
        node3d->SetRotation(kf.mRotation);
        node3d->SetScale(kf.mScale);
    }
    else if (target->IsWidget())
    {
        Widget* widget = static_cast<Widget*>(target);
        widget->SetPosition(glm::vec2(kf.mPosition.x, kf.mPosition.y));

        glm::vec3 euler = glm::degrees(glm::eulerAngles(kf.mRotation));
        widget->SetRotation(euler.z);

        widget->SetScale(glm::vec2(kf.mScale.x, kf.mScale.y));
    }
}
