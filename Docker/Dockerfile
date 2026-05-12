# Polyphase Engine - Universal Build Environment
#
# The editor is pre-compiled during image build and runs in headless mode
# to cook/package assets for each target platform.
#
# Usage:
#   Build image:
#     docker build -t polyphase-engine -f Docker/Dockerfile .
#
#   Build Editor (copy pre-built editor to output):
#     docker run --rm -v ./output:/game polyphase-engine build-editor
#
#   Build Game (requires project with .octp file):
#     docker run --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-linux
#     docker run --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-3ds
#     docker run --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-gcn
#     docker run --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-wii
#
#   Debug mode (drops to shell on failure):
#     docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-3ds --debug
#
#   Interactive:
#     docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine bash

# Inherit the prebuilt toolchain (Debian Bookworm + GCC 12 + Vulkan SDK +
# glslc + devkitPPC/ARM + libogc2 + curl-impersonate) from polyphase-bare.
# That image is built from Docker/Dockerfile.base and published manually
# from a workstation — see .github/workflows/release.yml's
# publish-docker-images job docstring for why CI doesn't rebuild it.
#
# Reinstalling the toolchain here (FROM debian:bookworm-slim + duplicated
# apt/wget/dkp-pacman steps) is what made this image's CI build flaky:
# `apt-get install devkitpro-pacman` runs a postinst that calls
# `dkp-pacman -Sy` against pkg.devkitpro.org, which routinely times out
# from GitHub-hosted runners ("failed retrieving file 'dkp-linux.db' ...
# Operation too slow"). polyphase-bare already paid that toll once
# locally; we just consume it.
#
# Forks publishing under a different namespace can override the base at
# build time:
#   docker build --build-arg BASE_IMAGE=myorg/polyphase-bare:latest -f Docker/Dockerfile .
ARG BASE_IMAGE=polyphaselabs/polyphase-bare:latest
FROM ${BASE_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# polyphase-bare already created these and set WORKDIR /polyphase; the
# explicit calls here are belt-and-braces so a future base swap that
# omits them doesn't silently break the COPY below.
RUN mkdir -p /polyphase /game /project /intermediate
WORKDIR /polyphase

# Copy source
COPY . /polyphase/

# Initialize submodules if needed
RUN if [ -f .gitmodules ]; then \
        git config --global --add safe.directory /polyphase && \
        git submodule update --init --recursive || true; \
    fi

# Compile shaders
WORKDIR /polyphase/Engine/Shaders/GLSL
RUN chmod +x compile.sh && ./compile.sh

WORKDIR /polyphase

# ============================================================================
# Pre-build prerequisites that the engine compile depends on.
#
# All three outputs below are gitignored (.gitignore: */Generated/EmbeddedAssets.*,
# libgit2 build artefacts, External/mbedtls/include/), so they DON'T ride along
# with `COPY . /polyphase/`. CI on bare metal generates them in the `build-linux`
# job; this image has to generate them here or downstream targets fail:
#
#   - Without libgit2 prebuild:  Linux Editor link fails on git2 symbols.
#   - Without asset stubs:       fatal error: ../Generated/EmbeddedAssets.h
#   - Without mbedTLS headers:   Wii/GCN game builds fail at runtime with
#                                "mbedtls/ssl.h: No such file or directory"
#                                inside HttpBackend_Dolphin.cpp.
#
# All three are baked at image build time so this image can build for every
# non-Windows target (Linux Editor + Linux Game + Wii + GCN + 3DS) on a
# fresh container without any further setup.
# ============================================================================

# 1. libgit2 prebuild — the Linux Editor link step pulls in libgit2.a from
#    External/lib/Lib/. cmake + libssl-dev are already in the base image.
RUN bash Tools/prebuild_libgit2.sh

# 2. Generated/EmbeddedAssets.h + .cpp — produced by the asset stub generator.
#    Embeds the engine's bundled assets as C arrays for the standalone build.
RUN python3 Tools/generate_embedded_stubs.py

# 3. mbedTLS source headers — vendored into External/mbedtls/include so that
#    Engine/Makefile_Wii and Makefile_GCN find <mbedtls/ssl.h> when the user
#    later runs `build-wii` or `build-gcn` against their game project.
#    External/mbedtls/.gitignore excludes include/ to keep the engine repo
#    small; the upstream tag pinned in External/mbedtls/README.md is the
#    source of truth for which version to vendor.
RUN cd External/mbedtls \
    && if [ ! -f include/mbedtls/ssl.h ]; then \
         git clone --depth 1 --branch v3.6.2 https://github.com/Mbed-TLS/mbedtls.git _clone \
         && mv _clone/include ./include \
         && cp _clone/LICENSE ./LICENSE 2>/dev/null || true \
         && rm -rf _clone \
         && cp polyphase_mbedtls_user_config.h.template include/mbedtls/polyphase_mbedtls_user_config.h ; \
       fi \
    && test -f include/mbedtls/ssl.h

# 4. libLua.a — Linux engine statically links Lua sources into libEngineEditor.a,
#    so the editor build doesn't need this. BUT native addons built inside the
#    container that want to bind into Lua expect to link against libLua.a at
#    External/Lua/Build/Linux/x64/ReleaseEditor/. Bake it so addon builds work
#    out of the box, matching the Windows installer's staging path.
#
#    CRITICAL: the upstream Lua Makefile compiles its .o files IN-TREE
#    (External/Lua/lapi.o, lauxlib.o, …). On x86-64 here, those would later
#    poison any Wii/GCN/3DS engine build whose Makefile VPATH includes
#    ../External/Lua — powerpc-eabi-ld rejects them with
#    "Relocations in generic ELF (EM: 62)". Do `make clean` after copying
#    the .a out; the staged copy at Build/Linux/x64/ReleaseEditor/ survives
#    because `make clean` only touches in-tree paths.
RUN make -C External/Lua a -j"$(nproc)" \
    && mkdir -p External/Lua/Build/Linux/x64/ReleaseEditor \
    && cp External/Lua/liblua.a External/Lua/Build/Linux/x64/ReleaseEditor/libLua.a \
    && make -C External/Lua clean \
    && test -f External/Lua/Build/Linux/x64/ReleaseEditor/libLua.a

# ============================================================================
# Pre-build Linux Editor (for headless asset cooking)
# ============================================================================
RUN cd /polyphase/Standalone && make -j$(nproc) -f Makefile_Linux_Editor
RUN cp /polyphase/Standalone/Build/Linux/PolyphaseEditor.elf /polyphase/PolyphaseEditor.elf

# ============================================================================
# Build Commands
# ============================================================================

# build-editor: Build the Linux Editor (and optionally copy to output)
RUN echo '#!/bin/bash\n\
set -e\n\
\n\
echo "Linux Editor already pre-built at /polyphase/PolyphaseEditor.elf"\n\
\n\
# Check for project (optional)\n\
if [ -d "/project" ] && [ -n "$(ls -A /project 2>/dev/null)" ]; then\n\
    OCTP_FILE=$(find /project -maxdepth 1 -name "*.octp" -print -quit)\n\
    echo "Project: /project"\n\
    [ -n "$OCTP_FILE" ] && echo "Project file: $OCTP_FILE"\n\
fi\n\
\n\
echo "Copying editor to /game..."\n\
cp /polyphase/PolyphaseEditor.elf /game/\n\
cp -r /polyphase/Engine /game/\n\
chmod -R 777 /game\n\
\n\
echo "Done! Editor at /game/PolyphaseEditor.elf"\n\
' > /usr/local/bin/build-editor && chmod +x /usr/local/bin/build-editor

# build-linux: Build Linux Game with embedded assets
RUN echo '#!/bin/bash\n\
\n\
DEBUG=0\n\
for arg in "$@"; do\n\
    case $arg in\n\
        --debug|-d) DEBUG=1 ;;\n\
    esac\n\
done\n\
\n\
handle_error() {\n\
    echo ""\n\
    echo "=== BUILD FAILED ==="\n\
    if [ $DEBUG -eq 1 ]; then\n\
        echo "Debug mode: dropping to shell. Type exit to quit."\n\
        exec /bin/bash\n\
    fi\n\
    exit 1\n\
}\n\
trap handle_error ERR\n\
set -e\n\
\n\
if [ ! -d "/project" ] || [ -z "$(ls -A /project 2>/dev/null)" ]; then\n\
    echo "Error: No project mounted at /project"\n\
    echo "Usage: docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-linux [--debug]"\n\
    exit 1\n\
fi\n\
\n\
OCTP_FILE=$(find /project -maxdepth 1 -name "*.octp" -print -quit)\n\
if [ -z "$OCTP_FILE" ]; then\n\
    echo "Error: No .octp project file found in /project"\n\
    exit 1\n\
fi\n\
\n\
PROJECT_NAME=$(basename "$OCTP_FILE" .octp)\n\
echo "Building Linux Game..."\n\
echo "Project: /project"\n\
echo "Project file: $OCTP_FILE"\n\
[ $DEBUG -eq 1 ] && echo "Debug mode: enabled"\n\
\n\
# Copy project to intermediate directory to protect original\n\
echo "Copying project to intermediate directory..."\n\
rm -rf /intermediate/*\n\
cp -r /project/* /intermediate/\n\
OCTP_FILE="/intermediate/$(basename $OCTP_FILE)"\n\
\n\
# Build using headless editor (cooks assets and compiles)\n\
cd /polyphase\n\
./PolyphaseEditor.elf -headless -project "$OCTP_FILE" -build Linux embedded\n\
\n\
echo "Copying to /game..."\n\
cp -r /intermediate/Packaged/Linux/* /game/\n\
chmod -R 777 /game\n\
\n\
echo "Done! Output at /game/"\n\
' > /usr/local/bin/build-linux && chmod +x /usr/local/bin/build-linux

# build-3ds: Build for Nintendo 3DS with embedded assets
RUN echo '#!/bin/bash\n\
\n\
DEBUG=0\n\
for arg in "$@"; do\n\
    case $arg in\n\
        --debug|-d) DEBUG=1 ;;\n\
    esac\n\
done\n\
\n\
handle_error() {\n\
    echo ""\n\
    echo "=== BUILD FAILED ==="\n\
    if [ $DEBUG -eq 1 ]; then\n\
        echo "Debug mode: dropping to shell. Type exit to quit."\n\
        exec /bin/bash\n\
    fi\n\
    exit 1\n\
}\n\
trap handle_error ERR\n\
set -e\n\
\n\
if ! command -v arm-none-eabi-gcc &> /dev/null; then\n\
    echo "Error: devkitARM not installed. Console builds unavailable."\n\
    exit 1\n\
fi\n\
\n\
if [ ! -d "/project" ] || [ -z "$(ls -A /project 2>/dev/null)" ]; then\n\
    echo "Error: No project mounted at /project"\n\
    echo "Usage: docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-3ds [--debug]"\n\
    exit 1\n\
fi\n\
\n\
OCTP_FILE=$(find /project -maxdepth 1 -name "*.octp" -print -quit)\n\
if [ -z "$OCTP_FILE" ]; then\n\
    echo "Error: No .octp project file found in /project"\n\
    exit 1\n\
fi\n\
\n\
PROJECT_NAME=$(basename "$OCTP_FILE" .octp)\n\
echo "Building for Nintendo 3DS..."\n\
echo "Project: /project"\n\
echo "Project file: $OCTP_FILE"\n\
[ $DEBUG -eq 1 ] && echo "Debug mode: enabled"\n\
\n\
# Copy project to intermediate directory to protect original\n\
echo "Copying project to intermediate directory..."\n\
rm -rf /intermediate/*\n\
cp -r /project/* /intermediate/\n\
OCTP_FILE="/intermediate/$(basename $OCTP_FILE)"\n\
\n\
# Build using headless editor (cooks assets and compiles)\n\
cd /polyphase\n\
./PolyphaseEditor.elf -headless -project "$OCTP_FILE" -build 3DS embedded\n\
\n\
echo "Copying ROM to /game..."\n\
cp /intermediate/Packaged/3DS/*.3dsx /game/\n\
chmod -R 777 /game\n\
\n\
echo "Done! 3DS build at /game/"\n\
' > /usr/local/bin/build-3ds && chmod +x /usr/local/bin/build-3ds

# build-gcn: Build for Nintendo GameCube with embedded assets
RUN echo '#!/bin/bash\n\
\n\
DEBUG=0\n\
for arg in "$@"; do\n\
    case $arg in\n\
        --debug|-d) DEBUG=1 ;;\n\
    esac\n\
done\n\
\n\
handle_error() {\n\
    echo ""\n\
    echo "=== BUILD FAILED ==="\n\
    if [ $DEBUG -eq 1 ]; then\n\
        echo "Debug mode: dropping to shell. Type exit to quit."\n\
        exec /bin/bash\n\
    fi\n\
    exit 1\n\
}\n\
trap handle_error ERR\n\
set -e\n\
\n\
if ! command -v powerpc-eabi-gcc &> /dev/null; then\n\
    echo "Error: devkitPPC not installed. Console builds unavailable."\n\
    exit 1\n\
fi\n\
\n\
if [ ! -d "/project" ] || [ -z "$(ls -A /project 2>/dev/null)" ]; then\n\
    echo "Error: No project mounted at /project"\n\
    echo "Usage: docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-gcn [--debug]"\n\
    exit 1\n\
fi\n\
\n\
OCTP_FILE=$(find /project -maxdepth 1 -name "*.octp" -print -quit)\n\
if [ -z "$OCTP_FILE" ]; then\n\
    echo "Error: No .octp project file found in /project"\n\
    exit 1\n\
fi\n\
\n\
PROJECT_NAME=$(basename "$OCTP_FILE" .octp)\n\
echo "Building for Nintendo GameCube..."\n\
echo "Project: /project"\n\
echo "Project file: $OCTP_FILE"\n\
[ $DEBUG -eq 1 ] && echo "Debug mode: enabled"\n\
\n\
# Copy project to intermediate directory to protect original\n\
echo "Copying project to intermediate directory..."\n\
rm -rf /intermediate/*\n\
cp -r /project/* /intermediate/\n\
OCTP_FILE="/intermediate/$(basename $OCTP_FILE)"\n\
\n\
# Build using headless editor (cooks assets and compiles)\n\
cd /polyphase\n\
./PolyphaseEditor.elf -headless -project "$OCTP_FILE" -build GameCube embedded\n\
\n\
echo "Copying ROM to /game..."\n\
cp -r /intermediate/Packaged/GameCube/*.dol /game/\n\
chmod -R 777 /game\n\
\n\
echo "Done! GameCube build at /game/"\n\
' > /usr/local/bin/build-gcn && chmod +x /usr/local/bin/build-gcn

# build-wii: Build for Nintendo Wii with embedded assets
RUN echo '#!/bin/bash\n\
\n\
DEBUG=0\n\
for arg in "$@"; do\n\
    case $arg in\n\
        --debug|-d) DEBUG=1 ;;\n\
    esac\n\
done\n\
\n\
handle_error() {\n\
    echo ""\n\
    echo "=== BUILD FAILED ==="\n\
    if [ $DEBUG -eq 1 ]; then\n\
        echo "Debug mode: dropping to shell. Type exit to quit."\n\
        exec /bin/bash\n\
    fi\n\
    exit 1\n\
}\n\
trap handle_error ERR\n\
set -e\n\
\n\
if ! command -v powerpc-eabi-gcc &> /dev/null; then\n\
    echo "Error: devkitPPC not installed. Console builds unavailable."\n\
    exit 1\n\
fi\n\
\n\
if [ ! -d "/project" ] || [ -z "$(ls -A /project 2>/dev/null)" ]; then\n\
    echo "Error: No project mounted at /project"\n\
    echo "Usage: docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-wii [--debug]"\n\
    exit 1\n\
fi\n\
\n\
OCTP_FILE=$(find /project -maxdepth 1 -name "*.octp" -print -quit)\n\
if [ -z "$OCTP_FILE" ]; then\n\
    echo "Error: No .octp project file found in /project"\n\
    exit 1\n\
fi\n\
\n\
PROJECT_NAME=$(basename "$OCTP_FILE" .octp)\n\
echo "Building for Nintendo Wii..."\n\
echo "Project: /project"\n\
echo "Project file: $OCTP_FILE"\n\
[ $DEBUG -eq 1 ] && echo "Debug mode: enabled"\n\
\n\
# Copy project to intermediate directory to protect original\n\
echo "Copying project to intermediate directory..."\n\
rm -rf /intermediate/*\n\
cp -r /project/* /intermediate/\n\
OCTP_FILE="/intermediate/$(basename $OCTP_FILE)"\n\
\n\
# Build using headless editor (cooks assets and compiles)\n\
cd /polyphase\n\
./PolyphaseEditor.elf -headless -project "$OCTP_FILE" -build Wii embedded\n\
\n\
echo "Copying ROM to /game..."\n\
cp -r /intermediate/Packaged/Wii/*.dol /game/\n\
chmod -R 777 /game\n\
\n\
echo "Done! Wii build at /game/"\n\
' > /usr/local/bin/build-wii && chmod +x /usr/local/bin/build-wii

# Help command
RUN echo '#!/bin/bash\n\
echo "Polyphase Engine Build Environment"\n\
echo "==================================="\n\
echo ""\n\
echo "The editor is pre-built and uses headless mode to cook assets."\n\
echo "Your project is copied to /intermediate before building to protect the original."\n\
echo ""\n\
echo "Commands:"\n\
echo "  build-editor  - Copy pre-built Linux Editor to output"\n\
echo "  build-linux   - Build Linux Game with embedded assets"\n\
echo "  build-3ds     - Build Nintendo 3DS with embedded assets"\n\
echo "  build-gcn     - Build Nintendo GameCube with embedded assets"\n\
echo "  build-wii     - Build Nintendo Wii with embedded assets"\n\
echo ""\n\
echo "Options:"\n\
echo "  --debug, -d   - Drop to shell on build failure (requires -it)"\n\
echo ""\n\
echo "Volumes:"\n\
echo "  /game         - Output directory (mount with -v ./output:/game)"\n\
echo "  /project      - Your game project with .octp file (mount with -v ./MyGame:/project)"\n\
echo "  /intermediate - (internal) Build working directory, protects /project from modification"\n\
echo ""\n\
echo "Examples:"\n\
echo "  docker run --rm -v ./output:/game polyphase-engine build-editor"\n\
echo "  docker run --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-linux"\n\
echo "  docker run -it --rm -v ./output:/game -v ./MyGame:/project polyphase-engine build-3ds --debug"\n\
' > /usr/local/bin/help && chmod +x /usr/local/bin/help

VOLUME /game
VOLUME /project

ENTRYPOINT []
CMD ["help"]
