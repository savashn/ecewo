cmake_minimum_required(VERSION 3.10)
project(deneme VERSION 0.1.0 LANGUAGES C)

# List of source files (do not delete this comment line)
set(APP_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/handlers.c
  PARENT_SCOPE
)
