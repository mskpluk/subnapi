#!/bin/sh
set -e

# Builds the .deb inside a pinned, rootless Podman container running
# Debian trixie's initial 13.0 release, instead of on the host (which
# tracks Debian testing/forky and is always newer). dpkg-shlibdeps bakes
# the actual glibc/Qt symbol versions used into the package's Depends:
# field at build time, so building on forky records a floor no released
# trixie point-release can satisfy. Building on 13.0 records the lowest
# floor the trixie series ever has, which every later point release (and,
# in practice, newer glibc/Qt/libmediainfo on Ubuntu 26.04 LTS) satisfies
# too. Confirmed by installing the resulting .deb fresh on trixie 13.0,
# trixie 13.6 and Ubuntu 26.04 LTS - all resolved cleanly, zero missing
# libs under ldd.
#
# Requires rootless Podman: `sudo apt install podman` once (interactively,
# in a real terminal - not through this script). Everything after that
# runs unprivileged.
#
# Usage: scripts/build-deb-trixie.sh [output-dir]
#   output-dir defaults to ../subnapi-deb-build (a sibling of the repo, so
#   build output never lands inside the git tree).
# Env: SKIP_VERIFY=1 skips the post-build install smoke test on trixie
#   (rolling) and Ubuntu 26.04.

cd "$(dirname "$0")/.."

BASE_IMAGE="docker.io/library/debian:trixie-20250811"
OUT_DIR="${1:-../subnapi-deb-build}"
CONTAINER_CMD=podman

if ! command -v "$CONTAINER_CMD" >/dev/null 2>&1; then
    echo "podman not found - install it first: sudo apt install podman" >&2
    exit 1
fi

VERSION=$(scripts/version.sh)

# The package is named from debian/changelog, so both have to agree; checked
# here rather than after a build that takes minutes. The output directory is
# not emptied between releases, so the package is picked out by this version,
# never by whatever `ls` happens to list first.
DEB_VERSION=$(sed -n '1s/^[^ ]* (\([^)]*\)).*/\1/p' debian/changelog)
DEB_FILE_VERSION=${DEB_VERSION#*:}   # dpkg leaves the epoch out of file names
if [ "${DEB_FILE_VERSION%-*}" != "$VERSION" ]; then
    echo "debian/changelog says $DEB_VERSION, version.h says $VERSION" >&2
    exit 1
fi
echo "Building subnapi $DEB_VERSION .deb against $BASE_IMAGE ..."

STAGE_DIR=$(mktemp -d)
trap 'rm -rf "$STAGE_DIR"' EXIT

# git ls-files gives a clean tree: no local build artifacts (Makefile,
# tmp/, .qmake.stash, libqnapi.a - gitignored but present on disk after a
# local qmake build) leaking into the container build.
git ls-files | while IFS= read -r f; do
    mkdir -p "$STAGE_DIR/$(dirname "$f")"
    cp "$f" "$STAGE_DIR/$f"
done

mkdir -p "$OUT_DIR"
OUT_DIR_ABS=$(cd "$OUT_DIR" && pwd)

"$CONTAINER_CMD" run --rm \
    -v "$STAGE_DIR:/src:ro" \
    -v "$OUT_DIR_ABS:/out" \
    "$BASE_IMAGE" \
    bash -c '
        set -e
        export DEBIAN_FRONTEND=noninteractive
        apt-get update -qq
        apt-get install -y -qq --no-install-recommends build-essential dpkg-dev
        apt-get build-dep -y -qq /src
        mkdir -p /build
        cp -r /src /build/src
        cd /build/src
        dpkg-buildpackage -us -uc -b
        cp /build/*.deb /out/
        cp /build/*.buildinfo /build/*.changes /out/ 2>/dev/null || true
    '

echo
echo "Built package(s):"
ls -la "$OUT_DIR"/subnapi*_"${DEB_FILE_VERSION}"_*.deb

DEB_MAIN="$OUT_DIR/subnapi_${DEB_FILE_VERSION}_amd64.deb"

if [ ! -f "$DEB_MAIN" ]; then
    echo "Package for version $DEB_VERSION not found at $DEB_MAIN" >&2
    exit 1
fi

if [ "$SKIP_VERIFY" = "1" ]; then
    exit 0
fi

echo
echo "Verifying $(basename "$DEB_MAIN") (version $DEB_VERSION from debian/changelog)"

verify_on() {
    image="$1"
    label="$2"
    echo
    echo "=== Verifying install on $label ($image) ==="
    "$CONTAINER_CMD" run --rm \
        -v "$OUT_DIR_ABS:/pkgs:ro" \
        "$image" \
        bash -c "
            set -e
            export DEBIAN_FRONTEND=noninteractive
            apt-get update -qq
            apt-get install -y -qq '/pkgs/$(basename "$DEB_MAIN")' >/dev/null
            subnapic --help | head -1
            missing=0
            for bin in /usr/bin/subnapi /usr/bin/subnapic; do
                if ldd \"\$bin\" | grep -q 'not found'; then
                    echo \"MISSING LIBS in \$bin:\"
                    ldd \"\$bin\" | grep 'not found'
                    missing=1
                fi
            done
            if [ \"\$missing\" = 0 ]; then
                echo 'OK: all shared libraries resolved'
            else
                exit 1
            fi
        "
}

verify_on "docker.io/library/debian:trixie" "Debian trixie (current point release)"
verify_on "docker.io/library/ubuntu:26.04" "Ubuntu 26.04 LTS"
