#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tlottie_dir=$(CDPATH= cd -- "$script_dir/../tlottie" && pwd)
dockerfile_dir="$script_dir/scripts/tlottie"
image_tag=tlottie-build
platform=linux/amd64

if [ ! -f "$tlottie_dir/Cargo.toml" ]; then
  echo "error: tlottie Cargo.toml not found at $tlottie_dir/Cargo.toml" >&2
  exit 1
fi

echo "Removing previous tlottie archives"
for abi in arm64-v8a armeabi-v7a x86 x86_64; do
  dest="$script_dir/$abi/libtlottie.a"
  if [ -f "$dest" ]; then
    rm -f "$dest"
    echo "  removed $dest"
  fi
  if [ ! -d "$script_dir/$abi" ]; then
    echo "error: output directory does not exist: $script_dir/$abi" >&2
    exit 1
  fi
done

echo "Building tlottie build image ($platform)"

docker build \
  --platform "$platform" \
  -t "$image_tag" \
  "$dockerfile_dir"

echo "Running tlottie build in container"
docker run --rm \
  --platform "$platform" \
  -v "$script_dir:/work" \
  -v "$tlottie_dir:/tlottie:ro" \
  -e HOST_UID="$(id -u)" \
  -e HOST_GID="$(id -g)" \
  "$image_tag"

echo "All tlottie Android archives are ready."