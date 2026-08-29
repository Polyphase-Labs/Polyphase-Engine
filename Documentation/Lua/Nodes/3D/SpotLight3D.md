# SpotLight3D

A node that emits light in a cone along the node's forward direction. Light intensity attenuates with distance like a point light, and also fades between the inner and outer cone angles.

Inheritance:
* [Node](../Node.md)
* [Node3D](Node3D.md)
* [Light3D](Light3D.md)
* [PointLight3D](PointLight3D.md)

---
### SetInnerAngle
Set the inner cone half-angle in degrees. Inside this angle the light is at full strength.

Sig: `SpotLight3D:SetInnerAngle(angle)`
 - Arg: `number angle` Inner cone half-angle in degrees
---
### GetInnerAngle
Get the inner cone half-angle in degrees.

Sig: `angle = SpotLight3D:GetInnerAngle()`
 - Ret: `number angle` Inner cone half-angle in degrees
---
### SetOuterAngle
Set the outer cone half-angle in degrees. Beyond this angle the light intensity is zero.

Sig: `SpotLight3D:SetOuterAngle(angle)`
 - Arg: `number angle` Outer cone half-angle in degrees
---
### GetOuterAngle
Get the outer cone half-angle in degrees.

Sig: `angle = SpotLight3D:GetOuterAngle()`
 - Ret: `number angle` Outer cone half-angle in degrees
---
### GetDirection
Get the direction the spot light is aiming (the node's forward vector).

Sig: `direction = SpotLight3D:GetDirection()`
 - Ret: `Vector direction` Light direction
---
### SetDirection
Set the direction the spot light is aiming.

Sig: `SpotLight3D:SetDirection(direction)`
 - Arg: `Vector direction` Light direction
---
