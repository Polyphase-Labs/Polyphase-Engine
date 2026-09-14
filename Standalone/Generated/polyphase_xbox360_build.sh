export DEVKITXENON=$HOME/devkitxenon
export PATH=$PATH:$DEVKITXENON/bin:$DEVKITXENON/usr/bin
cd "/mnt/m/Projects/Polyphase/Addons/BuildTargets/BuildTarget-Xbox360/"
make -f "Packages/com.polyphase.build.target.xbox360/Makefile_Xbox360" POLYPHASE_PATH="/mnt/m/Projects/Polyphase/Polyphase/CODE/mergePolyphase/polyphase-engine/" -j4
