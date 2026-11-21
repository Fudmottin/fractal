#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

case "${1:-debug}" in
   debug)
      CONFIG=Debug
      BUILD_DIR="$ROOT/build-debug"
      ;;
   release)
      CONFIG=Release
      BUILD_DIR="$ROOT/build-release"
      ;;
   *)
      echo "Usage: $0 [debug|release]" >&2
      exit 1
      ;;
esac

cmake -S "$ROOT" -B "$BUILD_DIR" \
   -DCMAKE_BUILD_TYPE="$CONFIG"

cmake --build "$BUILD_DIR" --parallel

