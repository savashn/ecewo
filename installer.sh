#!/usr/bin/env bash
set -euo pipefail

ADMIN=0
for arg in "$@"; do
  case $arg in
    --admin) ADMIN=1 ;;
    *)        echo "Unknown argument: $arg"; exit 1 ;;
  esac
done

FILE_URL="https://raw.githubusercontent.com/savashn/ecewo/main/cli.c"
MAKEFILE_URL="https://raw.githubusercontent.com/savashn/ecewo/main/makefile"

FILE_NAME="$(basename "$FILE_URL")"    # cli.c
MAKE_NAME="makefile"

echo "Downloading: $FILE_URL → $FILE_NAME"
curl -fSL -o "$FILE_NAME" "$FILE_URL"

echo "Downloading: $MAKEFILE_URL → $MAKE_NAME"
curl -fSL -o "$MAKE_NAME" "$MAKEFILE_URL"

# Run
if [[ $ADMIN -eq 1 ]]; then
  make install-admin
else
  make install
fi
