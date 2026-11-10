#!/bin/sh
# Prefer the sibling Native checkout for local development; otherwise clone
# into build/deps/native. Explicit NATIVE_URL/NATIVE_REF select the clone.
# --path only resolves the destination; --update refreshes the cached clone.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
url=${NATIVE_URL:-https://github.com/wischner/native.git}
ref=${NATIVE_REF:-master}
dest="$root/build/deps/native"
if test "${1:-}" != --update && test -z "${NATIVE_URL:-}${NATIVE_REF:-}" &&
   test -f "$root/../native/include/native.h"; then
    dest=$(CDPATH= cd -- "$root/../native" && pwd)
    echo "$dest"
    exit 0
fi
if test "${1:-}" = --path; then
    echo "$dest"
    exit 0
fi
if test ! -e "$dest/include/native.h"; then
    rm -rf -- "$dest"
    mkdir -p "$(dirname -- "$dest")"
    git clone --quiet --branch "$ref" "$url" "$dest" >&2
elif test "${1:-}" = --update; then
    git -C "$dest" fetch --quiet origin "$ref" >&2
    git -C "$dest" checkout --quiet --detach FETCH_HEAD >&2
fi
echo "$dest"
