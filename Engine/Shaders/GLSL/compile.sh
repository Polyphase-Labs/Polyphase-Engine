#!/bin/bash
# Compiles every GLSL shader in src/ to SPIR-V in bin/ using glslc from the
# Vulkan SDK. Output files keep the source name (bin/Forward.vert is SPIR-V).
mkdir -p bin
cd ./src

if [ -n "$VULKAN_SDK" ] && [ -x "$VULKAN_SDK/bin/glslc" ]; then
    GLSLC="$VULKAN_SDK/bin/glslc"
elif command -v glslc >/dev/null 2>&1; then
    GLSLC="$(command -v glslc)"
else
    echo "ERROR: glslc not detected - have you installed Shaderc? Try the LunarG Vulkan SDK!"
    exit 1
fi

FAILED=0
for file in *.vert *.frag *.comp
do
    echo "$GLSLC" "$file" -O -g -fpreserve-bindings -o "../bin/$file"
    if ! "$GLSLC" "$file" -O -g -fpreserve-bindings -o "../bin/$file"; then
        echo "ERROR: failed to compile $file"
        FAILED=1
    fi
done

cd ..

if [ "$FAILED" -ne 0 ]; then
    echo "Shader compilation FAILED."
    exit 1
fi
echo "Compilation successful!"
