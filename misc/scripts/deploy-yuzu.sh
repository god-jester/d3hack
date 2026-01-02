#!/bin/bash
set -e

# Verify the path is set.
if [ -z "${YUZU_PATH}" ]; then
    echo "YUZU_PATH appears to not be set! Check your exlaunch.sh?"
fi

# Setup the path to the game's mods folder.
export MODS_PATH=${YUZU_PATH}/${PROGRAM_ID}/d3hack

# Ensure directory exists.
mkdir -p ${MODS_PATH}/exefs;

# Copy over files.
cp ${OUT}/* ${MODS_PATH}/exefs

# Indicate completion so VSCode doesn't make us guess
echo "Deployed to Yuzu."