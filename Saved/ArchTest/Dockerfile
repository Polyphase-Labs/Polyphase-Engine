FROM archlinux:latest

ARG UID=1000
ARG GID=1000

# Layer 1: exactly what Documentation/Development/SetupEnvironment/Linux.md lists for Arch.
RUN pacman -Syu --noconfirm --needed \
      gcc make libx11 alsa-lib libpulse curl cmake pkgconf openssl \
 && pacman -Scc --noconfirm

# Layer 2: things the build/test needs that Linux.md does not list for Arch.
#   git/python/wget/xz  - clone, prebuild.sh, SDK download
#   vulkan-icd-loader   - Arch equivalent of the doc's apt libvulkan-dev
#   vulkan-swrast       - lavapipe software Vulkan for the headless runtime smoke test
#   xvfb/xwd/imagemagick - virtual display + screenshots
RUN pacman -S --noconfirm --needed \
      git python wget xz which \
      vulkan-icd-loader vulkan-headers vulkan-swrast vulkan-tools \
      xorg-server-xvfb xorg-xwd imagemagick \
 && pacman -Scc --noconfirm

ENV VULKAN_VERSION=1.4.350.0
RUN wget -q https://sdk.lunarg.com/sdk/download/${VULKAN_VERSION}/linux/vulkansdk-linux-x86_64-${VULKAN_VERSION}.tar.xz \
 && mkdir -p /opt/vulkan \
 && tar -xf vulkansdk-linux-x86_64-${VULKAN_VERSION}.tar.xz -C /opt/vulkan \
 && rm vulkansdk-linux-x86_64-${VULKAN_VERSION}.tar.xz
ENV VULKAN_SDK=/opt/vulkan/${VULKAN_VERSION}/x86_64
ENV PATH="${VULKAN_SDK}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${VULKAN_SDK}/lib"
ENV VK_LAYER_PATH="${VULKAN_SDK}/share/vulkan/explicit_layer.d"

RUN groupadd -g ${GID} builder && useradd -m -u ${UID} -g ${GID} builder \
 && mkdir -p /work /out && chown builder:builder /work /out
USER builder
WORKDIR /work
COPY --chown=builder:builder run_tests.sh /usr/local/bin/run_tests.sh
CMD ["bash", "/usr/local/bin/run_tests.sh"]
