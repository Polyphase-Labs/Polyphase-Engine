# Polyphase 3DS banner converter: glTF -> CGFX via pycgfx, plus scene lighting.
#
# Usage: python banner_cgfx.py <pycgfx dir> <in.gltf> <out.cgfx>
#
# pycgfx converts the model and creates one directional light ("TheLight")
# pointing down -Z with white colour, and leaves every material's ambient
# colour black, i.e. a headlight from the camera and no fill. The editor's
# BannerGltfExporter writes the scene's directional light and ambient colour
# into the glTF root "extras", and this script applies them after conversion:
#
#   "extras": { "polyphase": {
#       "lightDirection": [x, y, z],   # direction the light travels, world space
#       "lightColor":     [r, g, b],
#       "ambient":        [r, g, b]
#   } }
#
# Anything missing keeps pycgfx's default.
import os
import sys

if len(sys.argv) < 4:
    print("usage: banner_cgfx.py <pycgfx dir> <in.gltf> <out.cgfx>")
    sys.exit(2)

pycgfx_dir, in_gltf, out_cgfx = sys.argv[1], sys.argv[2], sys.argv[3]
sys.path.insert(0, pycgfx_dir)

import gltflib  # noqa: E402  (installed with pip alongside pycgfx)
from main import convert_gltf, write  # noqa: E402  (pycgfx's main.py)
from cgfx.shared import ColorFloat, Vector3  # noqa: E402


def clamp01(v):
    return max(0.0, min(1.0, float(v)))


gltf = gltflib.GLTF.load(in_gltf, load_file_resources=True)
cgfx = convert_gltf(gltf)

extras = gltf.model.extras if isinstance(gltf.model.extras, dict) else {}
settings = extras.get("polyphase", {}) if isinstance(extras, dict) else {}

light = cgfx.data.lights["TheLight"]
if light is not None:
    direction = settings.get("lightDirection")
    if direction and len(direction) == 3:
        light.position_or_direction = Vector3(float(direction[0]), float(direction[1]), float(direction[2]))
    color = settings.get("lightColor")
    if color and len(color) == 3:
        c = ColorFloat(clamp01(color[0]), clamp01(color[1]), clamp01(color[2]), 1.0)
        light.diffuse = c
        light.specular = [c, c]

ambient = settings.get("ambient")
if ambient and len(ambient) == 3:
    # The PICA ambient term is light.ambient (white) * material.ambient, so the
    # scene ambient goes on every material.
    mat_ambient = ColorFloat(clamp01(ambient[0]), clamp01(ambient[1]), clamp01(ambient[2]), 1.0)
    for model_name in cgfx.data.models:
        model = cgfx.data.models[model_name]
        for material_name in model.materials:
            model.materials[material_name].material_color.ambient = mat_ambient

with open(out_cgfx, "wb") as f:
    f.write(write(cgfx))
print(f"banner_cgfx: wrote {out_cgfx} ({os.path.getsize(out_cgfx)} bytes)")
