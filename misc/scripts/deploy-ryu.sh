#!/bin/bash
set -e

# Support two dev envs
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    export RYU_PATH="${RYU_MAC_PATH}"
else
    export RYU_PATH="${RYU_WIN_PATH}"
fi

# Verify the path is set.
if [ -z "${RYU_PATH}" ]; then
    echo "RYU_PATH appears to not be set! Check your exlaunch.sh?"
fi

# Setup the path to the game's mods folder.
export MODS_PATH=${RYU_PATH}/mods/contents/${PROGRAM_ID}/${NAME}

# Ensure directory exists.
mkdir -p "${MODS_PATH}/exefs";

# Copy over files.
cp ${OUT}/* "${MODS_PATH}/exefs"

# Indicate completion so VSCode doesn't make us guess
echo "Deployed to Ryujinx: "; date