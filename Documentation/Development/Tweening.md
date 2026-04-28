# Tweening System

The Tween system provides smooth value interpolation with easing functions. It supports scalar values, vectors, quaternions, and has built-in helpers for common node properties like position, rotation, scale, and color.

Tweens run on the engine's update loop (after TimerManager) and use callback functions to deliver interpolated values each frame.

## Quick Start

```lua
-- Tween a scalar value from 0 to 100 over 2 seconds
Tween.Value(Easing.InOutCubic, 0, 100, 2.0,
    function(value, pct, finished)
        Log.Debug("Value: " .. value .. " Progress: " .. pct)
    end,
    function()
        Log.Debug("Done!")
    end
)

-- Tween a node's position
Tween.Position(myNode, Easing.OutBack, Vec(10, 5, 0), 1.0, function()
    Log.Debug("Arrived!")
end)
```

## API Reference

### Tween.Value(easing, from, to, duration, onUpdate, [onComplete])

Tweens a scalar number. Returns a tween ID.

| Parameter | Type | Description |
|-----------|------|-------------|
| easing | Easing constant | Easing function to use |
| from | number | Start value |
| to | number | End value |
| duration | number | Duration in seconds |
| onUpdate | function(value, pct, finished) | Called each frame |
| onComplete | function() | Called once when finished (optional) |

```lua
local id = Tween.Value(Easing.OutQuad, 0, 255, 1.5,
    function(value, pct, finished)
        -- value: interpolated number
        -- pct: 0.0 to 1.0 progress
        -- finished: true on the last frame
    end
)
```

### Tween.Vector(easing, from, to, duration, onUpdate, [onComplete])

Tweens between two vectors (component-wise interpolation). Returns a tween ID.

```lua
Tween.Vector(Easing.InOutSine, Vec(0, 0, 0), Vec(100, 200, 50), 3.0,
    function(vec, pct, finished)
        myNode:SetPosition(vec)
    end
)
```

### Tween.Quaternion(easing, from, to, duration, onUpdate, [onComplete])

Identical to `Tween.Vector` but semantically indicates quaternion interpolation. Quaternions are represented as Vec4 in Lua.

### Tween.Position(node, easing, to, duration, [onComplete])

Tweens a Node3D's local position from its current value to the target.

```lua
Tween.Position(myNode, Easing.OutBack, Vec(10, 5, 0), 1.0, function()
    Log.Debug("Arrived!")
end)
```

### Tween.Rotation(node, easing, to, duration, [onComplete])

Tweens a Node3D's euler rotation from its current value to the target (in degrees).

```lua
Tween.Rotation(myNode, Easing.InOutQuad, Vec(0, 180, 0), 2.0)
```

### Tween.Scale(node, easing, to, duration, [onComplete])

Tweens a Node3D's scale from its current value to the target.

```lua
Tween.Scale(myNode, Easing.InOutElastic, Vec(2, 2, 2), 0.5)
```

### Tween.Color(node, easing, to, duration, [onComplete])

Tweens a Widget's color from its current value to the target RGBA.

```lua
Tween.Color(myWidget, Easing.OutExpo, Vec(1, 0, 0, 1), 0.3)
```

### Control Functions

```lua
Tween.Pause(id)      -- Pause a tween by ID
Tween.Resume(id)     -- Resume a paused tween
Tween.Cancel(id)     -- Cancel and remove a tween
Tween.CancelAll()    -- Cancel all active tweens
```

## Easing Constants

All easing functions are accessed through the `Easing` global table:

| Constant | Description |
|----------|-------------|
| `Easing.Linear` | No easing, constant speed |
| `Easing.InSine` | Sine ease in |
| `Easing.OutSine` | Sine ease out |
| `Easing.InOutSine` | Sine ease in-out |
| `Easing.InQuad` | Quadratic ease in |
| `Easing.OutQuad` | Quadratic ease out |
| `Easing.InOutQuad` | Quadratic ease in-out |
| `Easing.InCubic` | Cubic ease in |
| `Easing.OutCubic` | Cubic ease out |
| `Easing.InOutCubic` | Cubic ease in-out |
| `Easing.InQuart` | Quartic ease in |
| `Easing.OutQuart` | Quartic ease out |
| `Easing.InOutQuart` | Quartic ease in-out |
| `Easing.InQuint` | Quintic ease in |
| `Easing.OutQuint` | Quintic ease out |
| `Easing.InOutQuint` | Quintic ease in-out |
| `Easing.InExpo` | Exponential ease in |
| `Easing.OutExpo` | Exponential ease out |
| `Easing.InOutExpo` | Exponential ease in-out |
| `Easing.InCirc` | Circular ease in |
| `Easing.OutCirc` | Circular ease out |
| `Easing.InOutCirc` | Circular ease in-out |
| `Easing.InBack` | Back ease in (overshoots) |
| `Easing.OutBack` | Back ease out (overshoots) |
| `Easing.InOutBack` | Back ease in-out |
| `Easing.InElastic` | Elastic ease in |
| `Easing.OutElastic` | Elastic ease out |
| `Easing.InOutElastic` | Elastic ease in-out |
| `Easing.InBounce` | Bounce ease in |
| `Easing.OutBounce` | Bounce ease out |
| `Easing.InOutBounce` | Bounce ease in-out |

## Examples

### UI Fade In

```lua
function MyUI:Start()
    self:SetColor(Vec(1, 1, 1, 0))  -- Start transparent
    Tween.Color(self, Easing.OutQuad, Vec(1, 1, 1, 1), 0.5)
end
```

### Bounce Scale on Pickup

```lua
function Pickup:Collect()
    Tween.Scale(self, Easing.OutElastic, Vec(0, 0, 0), 0.6, function()
        self:Destroy()
    end)
end
```

### Chained Tweens

```lua
Tween.Position(myNode, Easing.OutQuad, Vec(10, 0, 0), 1.0, function()
    Tween.Position(myNode, Easing.InOutCubic, Vec(10, 10, 0), 1.0, function()
        Log.Debug("Both moves complete!")
    end)
end)
```

### Camera Shake with Value Tween

```lua
function CameraShake:Shake(intensity, duration)
    Tween.Value(Easing.OutExpo, intensity, 0, duration,
        function(value, pct, finished)
            local offsetX = (math.random() - 0.5) * 2 * value
            local offsetY = (math.random() - 0.5) * 2 * value
            self.camera:SetPosition(self.basePos + Vec(offsetX, offsetY, 0))
        end,
        function()
            self.camera:SetPosition(self.basePos)
        end
    )
end
```
