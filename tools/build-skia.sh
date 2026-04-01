#!/bin/bash
# build-skia.sh — Build Skia Graphite for Pulp
# Uses olilarkin/skia-builder to produce pre-built static libraries
#
# Usage:
#   ./tools/build-skia.sh          # Build for current platform
#   ./tools/build-skia.sh mac      # Build for macOS
#   ./tools/build-skia.sh ios      # Build for iOS
#   ./tools/build-skia.sh all      # Build all platforms
#
# Prerequisites: python3, ninja, git
# Output: external/skia-build/{platform}/lib/ + include/

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PULP_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_BUILDER_DIR="$PULP_ROOT/external/skia-builder"
SKIA_BUILD_OUTPUT="$PULP_ROOT/external/skia-build"
SKIA_BUILDER_REF="${SKIA_BUILDER_REF:-7eecb8abf1f77b2a8bac2e81c38e20708cb79c24}"
SKIA_BRANCH="${SKIA_BRANCH:-chrome/m144}"

PLATFORM="${1:-mac}"

echo "=== Pulp Skia Builder ==="
echo "Platform: $PLATFORM"
echo "Builder ref: $SKIA_BUILDER_REF"
echo "Branch: $SKIA_BRANCH"
echo "Output: $SKIA_BUILD_OUTPUT"
echo ""

# Clone skia-builder if not present
if [ ! -d "$SKIA_BUILDER_DIR" ]; then
    echo "Cloning skia-builder..."
    git clone https://github.com/olilarkin/skia-builder.git "$SKIA_BUILDER_DIR"
fi

echo "Syncing skia-builder to $SKIA_BUILDER_REF..."
git -C "$SKIA_BUILDER_DIR" fetch --depth 1 origin "$SKIA_BUILDER_REF"
git -C "$SKIA_BUILDER_DIR" checkout --detach "$SKIA_BUILDER_REF"

# Increase file limit on macOS
if [ "$(uname)" = "Darwin" ]; then
    ulimit -n 2048
fi

# Build
cd "$SKIA_BUILDER_DIR"

if [ "$PLATFORM" = "all" ]; then
    python3 build-skia.py mac -branch "$SKIA_BRANCH" --shallow
    # python3 build-skia.py ios -branch "$SKIA_BRANCH" --shallow
    # python3 build-skia.py linux -branch "$SKIA_BRANCH" --shallow
    # python3 build-skia.py win -branch "$SKIA_BRANCH" --shallow
else
    python3 build-skia.py "$PLATFORM" -branch "$SKIA_BRANCH" --shallow
fi

# Copy output to Pulp's expected location
echo ""
echo "Copying build output to $SKIA_BUILD_OUTPUT..."
mkdir -p "$SKIA_BUILD_OUTPUT"

# Copy headers
if [ -d "$SKIA_BUILDER_DIR/build/include" ]; then
    cp -R "$SKIA_BUILDER_DIR/build/include" "$SKIA_BUILD_OUTPUT/"
fi

# Copy platform libraries
for plat in mac ios win linux wasm; do
    if [ -d "$SKIA_BUILDER_DIR/build/$plat" ]; then
        mkdir -p "$SKIA_BUILD_OUTPUT/$plat"
        cp -R "$SKIA_BUILDER_DIR/build/$plat/lib" "$SKIA_BUILD_OUTPUT/$plat/"
    fi
done

echo ""
echo "=== Skia build complete ==="
echo "Set SKIA_DIR=$SKIA_BUILD_OUTPUT when configuring Pulp:"
echo "  cmake -B build -DSKIA_DIR=$SKIA_BUILD_OUTPUT"
