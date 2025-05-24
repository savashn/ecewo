#!/bin/bash

echo "ecewo - Build Script for Linux and MacOS"
echo "2025 (c) Savas Sahin <savashn>"
echo ""

FILE_URL="https://raw.githubusercontent.com/savashn/ecewo-plugins/main/ecewo.sh"
BASE_DIR="src"

curl -O "$FILE_URL"

echo "Create a project:"
read -p "Enter project name >>> " PROJECT_NAME
mkdir -p "$BASE_DIR"

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
    text(200, "hello world!");
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
    final_router();
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

rm -rf assets/
rm -f LICENSE README.md create.bat create.sh
