#!/bin/sh
set -eu

RELEASE_DIR=${XL_ZIP_RELEASE_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)}
TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/xlang-zip-link.XXXXXX")
trap 'rm -rf -- "$TEST_DIR"' EXIT HUP INT TERM

mkdir "$TEST_DIR/dest" "$TEST_DIR/outside" "$TEST_DIR/outside/nested"
ln -s ../outside "$TEST_DIR/dest/link"
ln -s ../outside/payload.txt "$TEST_DIR/dest/target.txt"
ln -s outside "$TEST_DIR/root-link"

cd "$RELEASE_DIR"
./xl run extlibs/zip/test/symlink_boundary.xl -- \
    "$TEST_DIR/dest" "$TEST_DIR/root-link" "$TEST_DIR/root-link/" \
    "$TEST_DIR/root-link/." "$TEST_DIR/root-link/./" "$TEST_DIR/dest/." \
    "$TEST_DIR/root-link/nested"
test -f "$TEST_DIR/dest/safe.txt"
test ! -e "$TEST_DIR/outside/payload.txt"
test ! -e "$TEST_DIR/outside/nested/payload.txt"
