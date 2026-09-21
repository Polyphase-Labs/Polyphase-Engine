# Transform Snapping & Precision

Snapping makes objects move, rotate and scale in fixed steps instead of freely, so things line up
without typing numbers into the Properties panel. Precision mode does the opposite job: it slows the
mouse down so you can make tiny adjustments by hand.

Both work everywhere you transform something in the editor:

- dragging the **gizmo handles** (the red / green / blue arrows, rings and boxes),
- the **cursor-locked transforms** (`R` rotate, `S` scale, and the move that starts after
  `Ctrl + D` duplicate; in the 2D viewport `G` / `R` / `S`),
- 3D nodes **and** 2D widgets.

## Cheat sheet

| Do this | To get this |
|---|---|
| Click the **magnet** button in the toolbar, or press `M` | Turn snapping on / off |
| Pick from the dropdown next to the magnet, or press `Alt + M` | Choose what to snap to: Increment, Vertex, Edge, Face |
| Hold `Shift` while transforming | Temporarily do the **opposite** of the magnet button |
| Hold `Ctrl` while transforming | Precision: the object moves much slower than the mouse |
| `Edit → Preferences → Appearance → Viewport` | Change the step sizes and the precision speeds |

`M` and `Alt + M` can be rebound in `Edit → Editor Hotkeys…` (category **Gizmo**). `Shift` and
`Ctrl` are fixed modifiers and cannot be rebound.

## Turning snapping on

The magnet button sits in the top toolbar, right after the **Local / World** button. It is
highlighted while snapping is on. The setting is remembered between editor sessions.

You do not have to switch it on to snap once in a while. `Shift` always **inverts** the button:

| Magnet button | `Shift` not held | `Shift` held |
|---|---|---|
| Off | moves freely | **snaps** |
| On | **snaps** | moves freely |

So most people leave the magnet off and just hold `Shift` when they want a snap, or leave it on and
hold `Shift` for the odd free adjustment. You can press and release `Shift` in the middle of a drag;
the object jumps to / from the snapped position straight away.

## What to snap to

The dropdown next to the magnet chooses the snap target.

### Increment (default)

Moves in fixed steps. With the default settings that is **1 unit** for translate, **15°** for rotate
and **0.1** for scale.

Steps are counted **from where the object was when you started the drag**, not from the world
origin. An object sitting at `x = 0.37` moved with a step of `1` goes to `1.37`, `2.37`, … not to
`1`, `2`. If you want an object exactly on the world grid, type the starting position once in the
Properties panel; after that, increment snapping keeps it on the grid.

### Vertex, Edge, Face

These snap the object to **other geometry in the scene**. They apply to **translate only**. Rotate
and scale always use increments, whatever the dropdown says.

What snaps is the object's **pivot** (its origin, the point the gizmo sits on), not its surface:

| Target | The pivot lands on… |
|---|---|
| **Face** | the point on the surface directly behind the pivot, as seen from the camera |
| **Vertex** | the nearest corner of the triangle behind the pivot |
| **Edge** | the nearest point on an edge of the triangle behind the pivot |

How to use it:

1. Turn snapping on (or hold `Shift`) and choose **Face**, **Vertex** or **Edge**.
2. Drag the object so its gizmo passes **over** the mesh you want to snap to.
3. A small orange marker shows where it will land: a **circle** for Face, a **square** for Vertex
   and Edge. Release the mouse to keep it.

Things worth knowing:

- **Vertex and Edge only grab when you are close.** The pivot has to be within the *Vertex / Edge
  Pick Radius* (16 pixels by default) of a corner or edge on screen. Further away, the object just
  moves freely, so large flat surfaces do not keep yanking the object to a far corner.
- **The pivot must be over a mesh.** If there is only empty space behind the pivot, nothing snaps.
  To reach a corner on the silhouette of a mesh, come at it from the inside of the mesh's outline.
- **Axis and plane handles are respected.** Dragging the red X arrow with Face snapping on slides
  the object along X until it lines up with the surface; it will not jump off the axis.
- **The object never snaps to itself** or to its own children, and hidden nodes are ignored.
- It works on static meshes, instanced meshes, terrain and voxel nodes, in both perspective and
  orthographic views.
- Because the pivot is what snaps, an object whose pivot is at its centre ends up half-buried in
  the surface. Objects with the pivot at their base (props, characters) sit on the surface as you
  would expect. `End` (drop to surface) is still the quickest way to place a centred object on the
  ground.
- The first snapped drag in a large scene can pause for a moment while the editor collects the
  scene's triangles. It is rebuilt at the start of each drag, so moved objects are always current.

### 2D widgets

In the 2D viewport, snapping always uses increments (the dropdown is ignored):

- **Offset** and **Size** snap to the *Widget Increment* (8 pixels by default).
- **Rotation** snaps to the same *Rotate Increment* as 3D.
- Axes set to **stretch** are stored as ratios rather than pixels, so they are left unsnapped.

## Precision (hold Ctrl)

Hold `Ctrl` during any transform and the object moves at a fraction of the mouse speed
(**0.1×** by default), which makes fine positioning easy without zooming in.

- On **gizmo handles** the handle falls behind the mouse cursor while `Ctrl` is held. That is
  expected: the mouse is being "geared down". Release `Ctrl` and it moves 1:1 again from where it
  is, with no jump.
- `Ctrl` and `Shift` combine: `Ctrl + Shift` with the magnet on gives you slow **and** free movement.
- Snapping still applies while `Ctrl` is held, you just need more mouse travel to reach the next
  step.
- While a transform is in progress, **Undo, Redo and Save shortcuts are paused**, so `Ctrl + Z`
  will not undo in the middle of a drag. (`Z` is also the "lock to Z axis" key.) They work again
  the moment you finish or cancel the transform.

> **Changed from earlier versions:** holding `Shift` used to slow cursor-locked transforms down.
> Slow movement is now on `Ctrl`, and `Shift` is the snapping modifier. `Shift + X / Y / Z` still
> locks to a plane.

## Axis locks and snapping together

`X`, `Y`, `Z` lock a cursor-locked transform to an axis, and `Shift + X / Y / Z` lock it to a plane.
Because `Shift` is also the snapping modifier, the order matters:

- Press `X` **first**, then hold `Shift` → locked to the X axis, with snapping inverted.
- Hold `Shift` first, then press `X` → locked to the **YZ plane** (as before).

Changing the lock restarts the transform from the object's original position.

## Settings

`Edit → Preferences → Appearance → Viewport`, under **Transform Snapping** and **Transform
Precision**. Scroll down; they are below the grid and selection colours.

| Setting | Default | What it does |
|---|---|---|
| Snapping Enabled | off | Same as the toolbar magnet button. |
| Snap To | Increment | Same as the toolbar dropdown. |
| Translate Increment | 1.0 | World units per step. `0` turns translate snapping off. |
| Rotate Increment | 15° | Degrees per step. Also used for widget rotation. `0` turns it off. |
| Scale Increment | 0.1 | Scale change per step. `0` turns it off. |
| Widget Increment | 8 px | Pixels per step for widget offset and size in the 2D viewport. |
| Vertex / Edge Pick Radius | 16 px | How close on screen the pivot must be before Vertex / Edge snapping grabs. |
| Precision → Normal | 1.0× | Mouse speed of cursor-locked transforms. Lower it if `R` / `S` feel twitchy. |
| Precision → While Control Is Held | 0.1× | Mouse speed while `Ctrl` is held (cursor-locked transforms and gizmo handles). |

*Normal* only affects the cursor-locked transforms. Gizmo handles always follow the cursor 1:1
unless `Ctrl` is held.

### A note on scale snapping

The two ways of scaling count steps slightly differently:

- **Gizmo scale handles** snap the scale *factor*: with a step of `0.1` an object of scale `2`
  goes to `2.2`, `2.4`, … (×1.1, ×1.2, …).
- **Cursor-locked `S`** snaps the *amount added*: the same object goes to `2.1`, `2.2`, ….

For an object that starts at scale `1` the two are identical.

## Troubleshooting

**Nothing snaps.** Check the magnet button and whether `Shift` is held (it inverts the button).
Check the increment for that operation is not `0`.

**Vertex / Edge does nothing but Face works.** You are further than the pick radius from a corner or
edge. Move closer, or raise *Vertex / Edge Pick Radius*.

**Face snapping does nothing.** The pivot is not over any mesh from the camera's point of view, or
the only mesh behind it is the object itself / one of its children.

**The object snaps to odd positions like 1.37.** Increments are counted from where the drag
started. Put the object on a round number once and it will stay on round numbers.

**`M` does nothing.** The mouse has to be over the viewport, and the key may have been rebound;
check `Edit → Editor Hotkeys…` → Gizmo → *Toggle Snapping*.

**The gizmo handle lags behind my mouse.** `Ctrl` is held (precision mode).

---

For how this is implemented, see [Transform Snapping internals](../Development/TransformSnapping.md).
