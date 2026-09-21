# Transform Snapping internals

How editor transform snapping and precision are put together, for people changing the editor. For
what the feature does and how to use it, read the user guide first:
[Transform Snapping & Precision](../Info/TransformSnapping.md).

Everything here is editor-only (`#if EDITOR`).

## Files

| File | Role |
|---|---|
| `Engine/Source/Editor/TransformSnap.h/.cpp` | The shared logic: is snapping active, increments, precision multipliers, surface query, on-screen marker |
| `Engine/Source/Editor/TriangleBvh.h/.cpp` | World-space triangle BVH + scene triangle gathering. Shared with the occlusion baker |
| `Engine/Source/Editor/Preferences/Appearance/Viewport/ViewportModule.h/.cpp` | The persisted settings and their Preferences UI |
| `Engine/Source/Editor/EditorImgui.cpp` | `DrawImGuizmo()` / `DrawImGuizmo2D()` (gizmo handles), toolbar magnet + dropdown in `DrawMainMenuBar()` |
| `Engine/Source/Editor/Viewport3d.cpp`, `Viewport2d.cpp` | Cursor-locked transforms (`HandleTransformControls`) |
| `Engine/Source/Editor/ImGuizmo/ImGuizmo.h/.cpp` | Vendored library, carries two small `// Polyphase:` patches |
| `Engine/Source/Editor/Hotkeys/EditorAction.h/.cpp` | `Gizmo_SnapToggle` (`M`), `Gizmo_SnapCycleMode` (`Alt + M`) |
| `Engine/Source/Editor/InputManager.cpp` | Suppresses Undo / Redo / Save while a transform is active |

## The two rules

```cpp
bool  TransformSnap::IsActive();                // snapping preference XOR IsShiftDown()
float TransformSnap::GetModalSpeedMultiplier(); // IsControlDown() ? precisionCtrl : precisionNormal
float TransformSnap::GetGizmoPrecisionScale();  // IsControlDown() ? precisionCtrl : 1.0
```

Every transform path asks these each frame, so `Shift` and `Ctrl` can be pressed or released in the
middle of a drag.

`Shift` and `Ctrl` are read with `IsShiftDown()` / `IsControlDown()`, **not** through
`EditorHotkeyMap`. A `KeyBinding` cannot be a bare modifier key: bindings use exact-modifier
matching, so a binding of "Shift" can never match while Shift is held. The toggle and the mode cycle
are normal remappable `EditorAction`s. They are polled in `HandleDefaultControls` **and** inside
`HandleTransformControls`, because default controls do not run during a cursor-locked transform.

`GetGizmoPrecisionScale()` deliberately ignores the *Normal* multiplier. A gizmo handle is supposed
to stay under the cursor; any value other than 1.0 would detach it permanently, not just while
`Ctrl` is held.

## There are two transform paths

The editor has two independent ways to move things, and each needs snapping wired in differently.

### 1. Gizmo handles (ImGuizmo)

`DrawImGuizmo()` calls `ImGuizmo::Manipulate(...)`. ImGuizmo tracks the cursor **absolutely** and
already supports increment snapping, so:

- **Increment** mode just passes `float snap[3]` as the snap argument (translate: units per axis,
  rotate: degrees in `snap[0]`, scale: factor in `snap[0]`). ImGuizmo rounds the cumulative delta
  from the drag start, in the local frame when the gizmo is in Local mode.
- **Vertex / Edge / Face** (translate only) cannot use that. ImGuizmo's axis-constrained result
  inherits its off-axis position, and its screen-size factor, from the matrix you feed it. Feeding
  back a surface-snapped position therefore corrupts the next frame. Instead the editor keeps a
  **shadow raw pivot** (`sRawPivot`): ImGuizmo is fed and updates the *unsnapped* position, and the
  nodes are placed from the snapped result:

  ```
  raw pivot --Manipulate--> new raw pivot --SnapToSurface--> hit
  delta = hit - dragStartPosition, projected onto the handle's axis / plane
  every node = translate(delta) * its drag-start matrix
  ```

  The matrix is read back **every frame**, not only when `Manipulate` returns true: that return
  value reports a change in the per-frame delta, not whether the matrix moved. This path only runs
  when the mode is not Increment, so default behaviour is untouched.

Undo works as before: original matrices are cached on the first frame of the drag and one
`EXE_EditTransforms` is pushed on release.

### 2. Cursor-locked transforms (`R`, `S`, move after duplicate; `G`/`R`/`S` in 2D)

These hide and lock the cursor and apply **mouse deltas** every frame. Rounding a per-frame delta
would never move anything, so they use a `TransformSnapAccum`:

1. Add this frame's (precision-scaled) delta to a **raw** total since the drag began.
2. Snap the raw total.
3. Apply the difference between the new snapped value and what was applied last frame. Translation
   is applied absolutely (`preTransform.position + target`), so snapped values are exact and do not
   accumulate float error.

The accumulator is reset in `SavePreTransforms()` and `RestorePreTransforms()`. That covers drag
start, commit, cancel, **and** an axis-lock change, since `HandleAxisLocking()` restores the
pre-transforms. The update also runs when `IsActive()` changes with a still mouse, so pressing
`Shift` takes effect immediately.

Details that are easy to break:

- **Rotation** snaps the accumulated angle about one axis. That is valid because the axis cannot
  change during the drag: the camera cannot move in these modes and a lock change resets the
  accumulator.
- **Scale**: the pivot branch divides `newScale / scale`. A snapped step can land a component on
  exactly `0`, which would produce NaN on the next frame, so a step that lands within `1e-4` of zero
  is held back and the following step carries the scale across zero.
- **Instanced mesh instances** get increment snapping only (the owning node is excluded from the
  surface query wholesale).
- **Widgets**: offset and size snap to the pixel increment; axes with `StretchX()` / `StretchY()`
  are ratios and are left unsnapped.

## Surface snapping (`TransformSnap::SnapToSurface`)

```cpp
bool SnapToSurface(Camera3D* camera, const std::vector<Node*>& excluded,
                   glm::vec3 rawPos, glm::vec3& outPos);
```

1. **Build the BVH lazily.** On the first frame of a drag that actually needs it, traverse the world
   from the root. `Node::Traverse` stops descending when the callback returns false, which is how
   invisible nodes and the selected nodes' whole subtrees are skipped. `Skybox3D` and `ShadowMesh3D`
   are ignored. `BeginDrag()` / `EndDrag()` drop the BVH, so it never outlives a drag. It stores
   triangles only, never node pointers, so it can go stale but not dangle.
2. **Cast one ray from the camera through the unsnapped pivot.** Perspective: from the camera
   position. Orthographic: from the pivot projected onto the camera plane, along the forward vector.
   Using the pivot, not the mouse, is what lets this work for cursor-locked transforms, where there
   is no visible cursor, and for gizmo handles alike.
3. **Face** returns the hit point. **Vertex** picks the closest of the hit triangle's three corners;
   **Edge** the closest point on its three edges. Both are accepted only within the pixel threshold,
   measured on screen with `Camera3D::WorldToScreenPosition` (device pixels, so the threshold is
   multiplied by `mEditorInterfaceScale`).
4. On success the marker position is stored; `DrawIndicator()` draws it on the ImGui foreground draw
   list for two frames, clipped to the viewport rectangle.

Known limitation: a pivot ray that misses all geometry does not snap, so a corner on a mesh's
silhouette has to be approached from inside the outline. A ring of probe rays on a miss would lift
this.

### TriangleBvh

Lifted out of `OcclusionBake/OcclusionBaker.cpp` so both users share one implementation.

- `GatherPrimitiveTriangles(Primitive3D*, std::vector<BvhTri>&)` handles `StaticMesh3D`,
  `InstancedMesh3D` (per instance), `Terrain3D` and `Voxel3D`. It calls `GetTransform()`, never
  `GetAABB()`, which can read a stale transform on freshly cloned trees.
- `StaticMesh::GetVertices()` **asserts** on vertex-colour meshes. Always branch on
  `HasVertexColor()` first and use `GetColorVertices()`; the gather does this.
- `Build()` reorders the triangles. Indices from `ClosestHit()` are only meaningful through
  `GetTri()` on the same built BVH.
- The occlusion baker keeps a thin `GatherOccluderTriangles()` wrapper that adds its own
  "material can occlude" test.

## The ImGuizmo patches

`ImGuizmo.cpp` is vendored. Each changed site is tagged `// Polyphase:` so the patch can be
re-applied after an upstream update.

**`ImGuizmo::SetPrecisionScale(float)`.** All three manipulations derive from one camera ray, which
comes from `io.MousePos`. While a manipulation is active the ray is built from a **virtual mouse**
instead:

```
virtualMouse += (io.MousePos - lastMousePos) * precisionScale   // once per frame
```

Because it accumulates scaled deltas instead of scaling an absolute position, the scale can change
mid-drag without a jump. It starts equal to the real mouse on the first frame of a manipulation and
is discarded when the manipulation ends. Activation and hover tests still use the real mouse.
Native snapping sits downstream of the ray, so it keeps working. The only other substitution is the
`io.MousePos.x` read in uniform scale.

**`ImGuizmo::GetTranslationConstraint(float* outDir)`** returns `0` free, `1` axis (`outDir` = axis)
or `2` plane (`outDir` = plane normal) for the current / last translate handle. It intentionally
does not check "is using": the manipulation flag is cleared inside `Manipulate` on the release
frame, and that frame still needs the constraint.

## Control is a transform modifier now

`InputManager::UpdateHotkeys()` polls Undo / Redo / Save with no knowledge of the viewport. With
`Ctrl` held for precision, pressing `Z` to lock the Z axis would also fire Undo in the middle of the
drag. Those actions are therefore skipped while `transforming` is true: a cursor-locked 3D or widget
transform is active, or `ImGuizmo::IsUsing()`.

If you add another global `Ctrl + key` action that would be destructive mid-drag, give it the same
guard.

## Settings

All in `ViewportModule` (`Appearance_Viewport.json`): `snapEnabled`, `snapMode`, `snapTranslate`,
`snapRotate`, `snapScale`, `snapWidgetPixels`, `snapPixelThreshold`, `precisionNormal`,
`precisionCtrl`. `SetSnapEnabled()` / `SetSnapMode()` save the module immediately, because they are
driven from the toolbar and hotkeys, not from the Preferences window's Apply button.

Read them through the `TransformSnap::Get*Increment()` helpers where possible, so the null-prefs
fallbacks stay in one place.

## Testing checklist

1. Magnet button and `M` toggle snapping, `Alt + M` cycles the mode, both also during a
   cursor-locked transform; the state survives an editor restart.
2. Gizmo translate / rotate / scale snap in World and Local mode; one undo entry per drag.
3. `Shift` inverts snapping in both toggle states, including with a still mouse.
4. `Ctrl` slows gizmo drags and cursor-locked transforms with no jump on press or release;
   `Ctrl + Z` / `Ctrl + S` do nothing mid-transform and work again afterwards.
5. `R` with 15° lands on exact multiples in pivot and local mode, with multi-select, and after an
   axis-lock change. `S` with 0.5 from scale 1 dragged far negative never produces NaN.
6. Face / Vertex / Edge in perspective and orthographic views, with axis and plane handles, onto
   instanced meshes, terrain, voxels and vertex-colour meshes; the marker lines up at an interface
   scale other than 1.
7. 2D: widget offset / size snap to the pixel increment, rotation snaps, stretched widgets are
   unaffected.
8. Occlusion bake output is unchanged (it shares `TriangleBvh`).
