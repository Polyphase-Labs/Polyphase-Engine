#!/bin/bash
# ==========================================================================
#  build_app_mac.sh
#  Stages the editor distribution and wraps it into dist/Polyphase.app.
#
#  Layout:
#    Polyphase.app/Contents/MacOS/Polyphase           editor binary
#    Polyphase.app/Contents/Resources/                the staged engine root
#                                                     (Engine/, Standalone/, External/, ...)
#    Polyphase.app/Contents/Frameworks/               libvulkan.1.dylib + libMoltenVK.dylib
#    Polyphase.app/Contents/Resources/vulkan/icd.d/   MoltenVK_icd.json (bundle-relative)
#
#  SYS_GetPolyphasePath() resolves to Contents/Resources when the binary lives
#  in Contents/MacOS, so the editor finds Engine/Assets etc. without a cwd.
#
#  Prerequisites:
#    - Xcode Command Line Tools (codesign, iconutil, sips, install_name_tool)
#    - Python 3
#    - Engine built: make -C Standalone -f Makefile_Mac_Editor (and Makefile_Mac_Game)
#    - VULKAN_SDK pointing at ~/VulkanSDK/<ver>/macOS (or an install under ~/VulkanSDK)
#
#  Environment (optional):
#    MAC_SIGN_IDENTITY   codesign identity; default "-" (ad-hoc)
#    MAC_NOTARY_PROFILE  notarytool keychain profile; notarizes when set with a real identity
#    SKIP_STAGE=1        reuse an existing dist/Editor
#
#  Usage: bash Installers/build_app_mac.sh
# ==========================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

IDENTITY="${MAC_SIGN_IDENTITY:--}"
NOTARY_PROFILE="${MAC_NOTARY_PROFILE:-}"

echo "============================================"
echo " Polyphase Engine - macOS App Bundle Builder"
echo "============================================"
echo ""

for tool in codesign iconutil sips install_name_tool plutil; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: $tool not found. Install the Xcode Command Line Tools: xcode-select --install" >&2
        exit 1
    fi
done

# --- Step 1: Stage distribution files ---
if [ "${SKIP_STAGE:-0}" != "1" ]; then
    echo "[1/5] Staging distribution files..."
    python3 Installers/stage_distribution.py --platform mac --verbose
else
    echo "[1/5] Reusing dist/Editor (SKIP_STAGE=1)"
fi
echo ""

if [ ! -f dist/Editor/PolyphaseEditor ]; then
    echo "ERROR: dist/Editor/PolyphaseEditor missing. Build the editor first:" >&2
    echo "       make -C Standalone -f Makefile_Mac_Editor -j\$(sysctl -n hw.ncpu)" >&2
    exit 1
fi
if [ ! -f dist/Editor/lib/libMoltenVK.dylib ] || [ ! -f dist/Editor/lib/libvulkan.1.dylib ]; then
    echo "ERROR: MoltenVK / Vulkan loader were not staged. Set VULKAN_SDK to ~/VulkanSDK/<ver>/macOS." >&2
    exit 1
fi

# --- Step 2: Version ---
VERSION="$(sed -n 's/^Version=//p' dist/Editor/version.txt | tr -d '[:space:]')"
VERSION_STRING="$(sed -n 's/^#define POLYPHASE_VERSION_STRING "\(.*\)"/\1/p' Engine/Source/Engine/Constants.h | head -n1)"
[ -z "$VERSION_STRING" ] && VERSION_STRING="$VERSION"
echo "[2/5] Assembling bundle (version ${VERSION_STRING})..."

APP="dist/Polyphase.app"
WORK="dist/mac-work"
rm -rf "$APP" "$WORK"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources/vulkan/icd.d" "$WORK/icon.iconset"

# Resources = the staged engine root.
rsync -a dist/Editor/ "$APP/Contents/Resources/"

# Binary into MacOS/.
mv "$APP/Contents/Resources/PolyphaseEditor" "$APP/Contents/MacOS/Polyphase"
chmod 0755 "$APP/Contents/MacOS/Polyphase"
chmod 0755 "$APP/Contents/Resources/Standalone/Build/Mac/Polyphase.macho" 2>/dev/null || true

# Vulkan runtime into Frameworks/ (+ any engine dylibs staged next to the binary).
mv "$APP/Contents/Resources/lib/libvulkan.1.dylib" "$APP/Contents/Resources/lib/libMoltenVK.dylib" "$APP/Contents/Frameworks/"
for f in "$APP"/Contents/Resources/*.dylib; do
    [ -f "$f" ] && mv "$f" "$APP/Contents/Frameworks/"
done

API_VERSION="1.2.0"
if [ -f "$APP/Contents/Resources/lib/MoltenVK_icd.json.sdk" ]; then
    v="$(sed -n 's/.*"api_version" *: *"\([^"]*\)".*/\1/p' "$APP/Contents/Resources/lib/MoltenVK_icd.json.sdk" | head -n1)"
    [ -n "$v" ] && API_VERSION="$v"
    rm -f "$APP/Contents/Resources/lib/MoltenVK_icd.json.sdk"
fi
cat > "$APP/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json" <<EOF
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../Frameworks/libMoltenVK.dylib",
        "api_version": "$API_VERSION",
        "is_portability_driver": true
    }
}
EOF

# --- Step 3: Info.plist, PkgInfo, icon ---
echo "[3/5] Writing Info.plist and icon..."
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>Polyphase</string>
	<key>CFBundleIdentifier</key>
	<string>com.polyphase.editor</string>
	<key>CFBundleName</key>
	<string>Polyphase</string>
	<key>CFBundleDisplayName</key>
	<string>Polyphase Engine</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleShortVersionString</key>
	<string>$VERSION_STRING</string>
	<key>CFBundleVersion</key>
	<string>$VERSION</string>
	<key>CFBundleIconFile</key>
	<string>Polyphase.icns</string>
	<key>LSMinimumSystemVersion</key>
	<string>12.0</string>
	<key>LSApplicationCategoryType</key>
	<string>public.app-category.developer-tools</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSSupportsAutomaticGraphicsSwitching</key>
	<true/>
	<key>CFBundleDocumentTypes</key>
	<array>
		<dict>
			<key>CFBundleTypeName</key>
			<string>Polyphase Project</string>
			<key>CFBundleTypeRole</key>
			<string>Editor</string>
			<key>CFBundleTypeIconFile</key>
			<string>Polyphase.icns</string>
			<key>LSItemContentTypes</key>
			<array>
				<string>com.polyphase.project</string>
			</array>
			<key>LSHandlerRank</key>
			<string>Owner</string>
		</dict>
	</array>
	<key>UTExportedTypeDeclarations</key>
	<array>
		<dict>
			<key>UTTypeIdentifier</key>
			<string>com.polyphase.project</string>
			<key>UTTypeDescription</key>
			<string>Polyphase Engine Project</string>
			<key>UTTypeConformsTo</key>
			<array>
				<string>public.data</string>
			</array>
			<key>UTTypeTagSpecification</key>
			<dict>
				<key>public.filename-extension</key>
				<array>
					<string>octp</string>
				</array>
				<key>public.mime-type</key>
				<array>
					<string>application/x-polyphase-project</string>
				</array>
			</dict>
		</dict>
	</array>
</dict>
</plist>
EOF
plutil -lint "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"

ICON_SRC="PolyphaseLogo_256.png"
for sz in 16 32 128 256; do
    sips -z $sz $sz "$ICON_SRC" --out "$WORK/icon.iconset/icon_${sz}x${sz}.png" >/dev/null
    dbl=$((sz * 2))
    if [ $dbl -le 256 ]; then
        sips -z $dbl $dbl "$ICON_SRC" --out "$WORK/icon.iconset/icon_${sz}x${sz}@2x.png" >/dev/null
    fi
done
iconutil -c icns "$WORK/icon.iconset" -o "$APP/Contents/Resources/Polyphase.icns"

# --- Step 4: rpath / install names, then sign inside-out ---
echo "[4/5] Fixing install names and signing (${IDENTITY})..."
install_name_tool -id @rpath/libvulkan.1.dylib "$APP/Contents/Frameworks/libvulkan.1.dylib" 2>/dev/null || true
VKREF="$(otool -L "$APP/Contents/MacOS/Polyphase" | awk '/libvulkan/{print $1; exit}')"
if [ -n "$VKREF" ] && [ "$VKREF" != "@rpath/libvulkan.1.dylib" ]; then
    install_name_tool -change "$VKREF" @rpath/libvulkan.1.dylib "$APP/Contents/MacOS/Polyphase"
fi
if ! otool -l "$APP/Contents/MacOS/Polyphase" | grep -q '@executable_path/../Frameworks'; then
    install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/Polyphase"
fi

SIGNFLAGS=()
if [ "$IDENTITY" != "-" ]; then
    cat > "$WORK/entitlements.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>com.apple.security.cs.disable-library-validation</key>
	<true/>
	<key>com.apple.security.cs.allow-unsigned-executable-memory</key>
	<true/>
	<key>com.apple.security.cs.allow-dyld-environment-variables</key>
	<true/>
</dict>
</plist>
EOF
    SIGNFLAGS=(--options runtime --timestamp --entitlements "$WORK/entitlements.plist")
fi
for f in "$APP"/Contents/Frameworks/*.dylib; do
    [ -f "$f" ] && codesign --force --sign "$IDENTITY" "${SIGNFLAGS[@]}" "$f"
done
# The staged game runtime binary is shipped as data but is still a Mach-O;
# sign it so Gatekeeper doesn't reject the bundle as containing unsigned code.
if [ -f "$APP/Contents/Resources/Standalone/Build/Mac/Polyphase.macho" ]; then
    codesign --force --sign "$IDENTITY" "${SIGNFLAGS[@]}" "$APP/Contents/Resources/Standalone/Build/Mac/Polyphase.macho"
fi
codesign --force --sign "$IDENTITY" "${SIGNFLAGS[@]}" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

if [ -n "$NOTARY_PROFILE" ] && [ "$IDENTITY" != "-" ]; then
    echo "Notarizing..."
    ditto -c -k --keepParent "$APP" "$WORK/notary.zip"
    xcrun notarytool submit "$WORK/notary.zip" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$APP"
fi

# --- Step 5: Done ---
echo "[5/5] Done."
rm -rf "$WORK"
echo ""
echo "============================================"
echo " App bundle built successfully!"
echo " Output: $APP"
echo ""
echo " Run:    open -n $APP"
echo " Note:   ad-hoc signed builds show a Gatekeeper warning after download;"
echo "         set MAC_SIGN_IDENTITY / MAC_NOTARY_PROFILE for distribution."
echo "============================================"
