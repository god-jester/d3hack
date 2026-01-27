#!/bin/bash
set -e

: "${BUILD_DIR:?BUILD_DIR not set}"

# Refresh "Built @" timestamp.
rm -f \
    "${BUILD_DIR}/CMakeFiles/d3hack.dir/source/program/main.cpp.obj" \
    "${BUILD_DIR}/CMakeFiles/d3hack.dir/source/program/gui2/ui/overlay.cpp.obj"
