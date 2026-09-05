#pragma once

#if EDITOR

// Editor-side bake for the scene's OcclusionData (see OcclusionData.h).
// Both functions must be called from the deferred end-of-frame dispatcher in
// EditorMain.cpp because they drive the EditorProgress modal.
namespace OcclusionBaker
{
    void BakeCurrentScene();
    void ClearCurrentScene();
}

#endif
