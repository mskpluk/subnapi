#!/bin/sh
set -e

cd "$(dirname "$0")/.."

VERSION=$(scripts/version.sh)
if [ -z "$VERSION" ]; then
    echo "Could not determine version from libqnapi/src/version.h" >&2
    exit 1
fi

DST_DIR="../subnapi-$VERSION"

if [ -e Makefile ]; then
    make distclean
fi

rm -rf "$DST_DIR"
mkdir "$DST_DIR"

# git ls-files enumerates exactly the tracked source tree: no .git
# metadata, no build artifacts, no accidentally-skipped dotfiles, and
# (since binaries with no source were removed from the tree) no
# manual per-file exclusion list to keep in sync by hand. debian/ is
# excluded separately - this project's Debian archive packaging lives
# in its own repository, and shipping a debian/ directory inside an
# upstream source tarball conflicts with that workflow.
git ls-files | grep -v '^debian/' | while IFS= read -r f; do
    mkdir -p "$DST_DIR/$(dirname "$f")"
    cp "$f" "$DST_DIR/$f"
done

tar -zcf "../subnapi-$VERSION.tar.gz" -C .. "subnapi-$VERSION"
rm -rf "$DST_DIR"

echo "Created ../subnapi-$VERSION.tar.gz"
