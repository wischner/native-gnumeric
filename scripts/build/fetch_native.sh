#!/bin/sh
# Clone Native from GitHub into build/deps/native unless it is already there,
# then print its path. NATIVE_URL and NATIVE_REF override the defaults;
# --update fetches and checks out NATIVE_REF again in an existing clone.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
url=${NATIVE_URL:-https://github.com/wischner/native.git}
ref=${NATIVE_REF:-fyodor-native-extensions}
dest="$root/build/deps/native"
if test ! -e "$dest/include/native.h"; then
    rm -rf -- "$dest"
    mkdir -p "$(dirname -- "$dest")"
    git clone --quiet --branch "$ref" "$url" "$dest" >&2
elif test "${1:-}" = --update; then
    git -C "$dest" fetch --quiet origin "$ref" >&2
    git -C "$dest" checkout --quiet --detach FETCH_HEAD >&2
fi
echo "$dest"
