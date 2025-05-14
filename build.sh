#!/bin/bash

echo "ecewo - Build Script for Linux and macOS"
echo "2025 (c) Savas Sahin <savashn>"
echo ""

# Define repository information
REPO="https://github.com/savashn/ecewo"

BASE_DIR="$(cd "$(dirname "$0")" && pwd)/"

# Initialize flags
RUN=0
REBUILD=0
UPDATE=0
CREATE=0
MIGRATE=0

# Parse command line arguments
for arg in "$@"; do
  case $arg in
    --run)
      RUN=1
      ;;
    --rebuild)
      REBUILD=1
      ;;
    --update)
      UPDATE=1
      ;;
    --create)
      CREATE=1
      ;;
    --migrate)
      MIGRATE=1
      ;;
    *)
      echo "Unknown argument: $arg"
      ;;
  esac
done

# Check if no parameters were provided
if [[ $RUN -eq 0 && $REBUILD -eq 0 && $UPDATE -eq 0 && $CREATE -eq 0 && $MIGRATE -eq 0 ]]; then
  echo "No parameters specified. Please use one of the following:"
  echo ==========================================================
  echo "  --run       # Build and run the project"
  echo "  --rebuild   # Build from scratch"
  echo "  --update    # Update Ecewo"
  echo "  --create    # Create a starter project"
  echo "  --migrate   # Migrate the "CMakeLists.txt" file"
  echo ==========================================================
  exit 0
fi

if [[ $CREATE -eq 1 ]]; then
  echo "Create a project:"
  read -p "Enter project name >>> " PROJECT_NAME
  mkdir -p src

  cat <<EOF > src/handlers.h
#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo.h"

void hello_world(Req *req, Res *res);

#endif
EOF

  cat <<EOF > src/handlers.c
#include "handlers.h"

void hello_world(Req *req, Res *res)
{
    reply(res, "200 OK", "text/plain", "hello world!");
}
EOF

  cat <<EOF > src/main.c
#include "server.h"
#include "handlers.h"

int main()
{
    init_router();
    get("/", hello_world);
    ecewo(4000);
    free_router();
    return 0;
}
EOF

  cat <<EOF > src/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES C)

set(APP_SRC
    \${CMAKE_CURRENT_SOURCE_DIR}/main.c
    \${CMAKE_CURRENT_SOURCE_DIR}/handlers.c
    PARENT_SCOPE
)
EOF

  echo "Starter project created successfully."
  exit 0
fi

# Build and run
if [[ $RUN -eq 1 ]]; then
  # Create build directory if it doesn't exist
  mkdir -p build
  
  cd build
  echo "Configuring with CMake..."
  cmake -G "Unix Makefiles" ..
  
  # Build the project
  echo "Building..."
  cmake --build . --config Release
  
  echo "Build completed!"
  echo ""
  echo "Running ecewo server..."
  
  # Check if server binary exists
  if [ -f ./server ]; then
    ./server
  else
    echo "Server executable not found. Check for build errors."
  fi
  
  # Return to original directory
  cd ..
  exit 0
fi

# If update requested, perform only update then exit
if [[ $UPDATE -eq 1 ]]; then
  echo "Updating from $REPO (branch: main)"
  rm -rf temp_repo
  mkdir -p temp_repo
  echo "Cloning repository..."
  git clone --depth 1 --branch main "$REPO" temp_repo || {
    echo "Clone failed. Check internet or branch name."
    rm -rf temp_repo
    exit 1
  }
  
  if [ -d temp_repo/.git ]; then
    rm -rf temp_repo/.git
  fi
  
  echo "Copying files..."
  # Use rsync to copy files with exclusions
#   rsync -av --exclude=build --exclude=temp_repo --exclude=*.sh --exclude=LICENSE --exclude=README.md temp_repo/ ./
  
#   rm -rf temp_repo
#   echo "Update complete."

    echo "Packing updated files..."
    tar --exclude=build \
        --exclude=temp_repo \
        --exclude='*.sh' \
        --exclude=LICENSE \
        --exclude=README.md \
        -cf temp_repo.tar -C temp_repo .

    echo "Unpacking into project directory..."
    tar -xf temp_repo.tar -C .

    rm -rf temp_repo temp_repo.tar
    echo "Update complete."
  exit 0
fi

# Rebuild
if [[ $REBUILD -eq 1 ]]; then
  echo "Cleaning build directory..."
  rm -rf build
  echo "Cleaned."
  echo ""
  # Create build directory if it doesn't exist
  mkdir -p build
  
  cd build
  echo "Configuring with CMake..."
  cmake -G "Unix Makefiles" ..
  
  # Build the project
  echo "Building..."
  cmake --build . --config Release
  
  echo "Build completed!"
  echo ""
  echo "Running ecewo server..."
  
  # Check if server binary exists
  if [ -f ./server ]; then
    ./server
  else
    echo "Server executable not found. Check for build errors."
  fi
  
  # Return to original directory
  cd ..
  exit 0
fi

if [ "$MIGRATE" = "1" ]; then
  echo "Migrating all .c files in src/ and its subdirectories to src/CMakeLists.txt"
  SRC_DIR="${BASE_DIR}src"
  CMAKE_FILE="$SRC_DIR/CMakeLists.txt"

  if [ ! -d "$SRC_DIR" ]; then
    echo "ERROR: Source directory '$SRC_DIR' not found!"
    exit 1
  fi

  # Keep the APP_SRC in a temporary file
  TMP_FILE=$(mktemp)
  {
    echo "set(APP_SRC"
    find "$SRC_DIR" -type f -name "*.c" | while read -r file; do
      REL_PATH="${file#$SRC_DIR}"
      echo "    \${CMAKE_CURRENT_SOURCE_DIR}${REL_PATH}"
    done
    echo "    PARENT_SCOPE"
    echo ")"
  } > "$TMP_FILE"

  # Clean up the old APP_SRC and add the new one
  awk '
    BEGIN { skip=0 }
    /set\(APP_SRC/ { skip=1 }
    skip && /\)/ { skip=0; next }
    skip { next }
    { print }
  ' "$CMAKE_FILE" > "${CMAKE_FILE}.tmp"

  cat "$TMP_FILE" >> "${CMAKE_FILE}.tmp"
  mv "${CMAKE_FILE}.tmp" "$CMAKE_FILE"
  rm "$TMP_FILE"

  echo "Migration complete."
  exit 0
fi

exit 0
