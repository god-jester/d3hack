#!/bin/bash
set -e

CLANGD_HPP=${MISC_PATH}/clangd_compat.hpp

mkdir -p ${BUILD_DIR}

# Copy clangd compat header if exists
if [ -f $CLANGD_HPP ]; then
    cp "$CLANGD_HPP" "$BUILD_DIR"
fi
