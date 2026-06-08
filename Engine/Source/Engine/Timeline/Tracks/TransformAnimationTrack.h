#pragma once

#include "Timeline/TimelineTrack.h"

class TransformAnimationTrack : public TimelineTrack
{
public:

    DECLARE_TRACK(TransformAnimationTrack, TimelineTrack);

    TransformAnimationTrack();
    virtual ~TransformAnimationTrack();

    virtual void Evaluate(float time, Node* target, TimelineInstance* inst) override;
    virtual void Reset(Node* target, TimelineInstance* inst) override;

    virtual const char* GetTrackTypeName() const override { return "TransformAnimation"; }
    virtual glm::vec4 GetTrackColor() const override;
    virtual TypeId GetDefaultClipType() const override;
};
