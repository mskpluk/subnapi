#!/bin/sh
set -e

# Builds and installs Subnapi as a per-user Flatpak from
# flatpak/io.github.mskpluk.subnapi.yml.
#
# One-time host setup (needs a real terminal for sudo):
#   sudo apt install flatpak flatpak-builder
#
# Everything below runs unprivileged (--user).
#
# Usage: scripts/build-flatpak.sh [build-dir]
#   build-dir defaults to ../subnapi-flatpak-build (a sibling of the
#   repo, so build output never lands inside the git tree).
# Env: BUNDLE=1 additionally creates a distributable single-file
#   subnapi-<version>.flatpak bundle in the build dir - installable
#   anywhere with `flatpak install ./subnapi-<version>.flatpak` (the
#   runtime is fetched from Flathub automatically).

cd "$(dirname "$0")/.."

MANIFEST=flatpak/io.github.mskpluk.subnapi.yml
BUILD_DIR="${1:-../subnapi-flatpak-build}"
RUNTIME_BRANCH=6.11

for tool in flatpak flatpak-builder; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool not found - install it first: sudo apt install flatpak flatpak-builder" >&2
        exit 1
    fi
done

flatpak remote-add --user --if-not-exists flathub \
    https://dl.flathub.org/repo/flathub.flatpakrepo

flatpak install --user -y --noninteractive flathub \
    org.kde.Platform//$RUNTIME_BRANCH org.kde.Sdk//$RUNTIME_BRANCH

flatpak-builder --user --install --force-clean \
    --state-dir="$BUILD_DIR/.flatpak-builder" \
    --repo="$BUILD_DIR/repo" \
    "$BUILD_DIR/build" "$MANIFEST"

echo
echo "Installed. Run with: flatpak run io.github.mskpluk.subnapi"

if [ "$BUNDLE" = "1" ]; then
    VERSION=$(scripts/version.sh)
    BUNDLE_FILE="$BUILD_DIR/subnapi-$VERSION.flatpak"
    flatpak build-bundle \
        --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
        "$BUILD_DIR/repo" "$BUNDLE_FILE" io.github.mskpluk.subnapi
    echo "Bundle created: $BUNDLE_FILE"
fi
