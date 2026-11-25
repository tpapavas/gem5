#!/usr/bin/env bash
set -euo pipefail

GEM5_ROOT="$(pwd)"
RAW_ROOT="$(dirname "$GEM5_ROOT")/Raw"

TORCH_SRC="$RAW_ROOT/.venv/lib/python3.8/site-packages/torch"
YAML_SRC="$RAW_ROOT/yaml-cpp-install"

TORCH_DST="$GEM5_ROOT/ext/ucie/torch"
YAML_DST="$GEM5_ROOT/ext/ucie/yaml-cpp"

echo "=================================="
echo " Installing UCIe dependencies"
echo "=================================="

# Check existing installations
test -f "$TORCH_SRC/include/torch/script.h" || {
    echo "ERROR: Torch headers not found"
    exit 1
}

test -f "$TORCH_SRC/lib/libtorch.so" || {
    echo "ERROR: LibTorch library not found"
    exit 1
}

test -f "$YAML_SRC/include/yaml-cpp/yaml.h" || {
    echo "ERROR: yaml-cpp headers not found"
    exit 1
}

test -f "$YAML_SRC/lib/libyaml-cpp.a" || {
    echo "ERROR: yaml-cpp static library not found"
    exit 1
}

mkdir -p "$TORCH_DST" "$YAML_DST"

echo "[1/3] Copying LibTorch..."

rsync -a --info=progress2 \
    "$TORCH_SRC/include/" "$TORCH_DST/include/"

rsync -a --info=progress2 \
    "$TORCH_SRC/lib/" "$TORCH_DST/lib/"

rsync -a \
    "$TORCH_SRC/share/" "$TORCH_DST/share/"

echo "[2/3] Copying yaml-cpp..."

rsync -a \
    "$YAML_SRC/include/" "$YAML_DST/include/"

rsync -a \
    "$YAML_SRC/lib/" "$YAML_DST/lib/"

rsync -a \
    "$YAML_SRC/share/" "$YAML_DST/share/"

echo "[3/3] Verifying installation..."

test -f "$TORCH_DST/include/torch/script.h"
test -f "$TORCH_DST/lib/libtorch.so"
test -f "$YAML_DST/include/yaml-cpp/yaml.h"
test -f "$YAML_DST/lib/libyaml-cpp.a"

# Prevent accidentally committing large dependencies
EXCLUDE="$GEM5_ROOT/.git/info/exclude"

if [ -f "$EXCLUDE" ]; then
    for pattern in \
        "/ext/ucie/torch/" \
        "/ext/ucie/yaml-cpp/"
    do
        grep -qxF "$pattern" "$EXCLUDE" || \
            printf '%s\n' "$pattern" >> "$EXCLUDE"
    done
fi

echo
echo "=================================="
echo " UCIe dependencies installed"
echo "=================================="
echo "Torch: $TORCH_DST"
echo "YAML : $YAML_DST"