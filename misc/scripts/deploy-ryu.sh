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

# Avoid path mismatches in Ryujinx (some code paths treat duplicate slashes as distinct).
export RYU_PATH="${RYU_PATH%/}"

# Setup the path to the game's mods folder.
export MODS_PATH=${RYU_PATH}/mods/contents/${PROGRAM_ID}/${NAME}

# Ensure directory exists.
mkdir -p "${MODS_PATH}/exefs";
mkdir -p "${MODS_PATH}/romfs";

# Copy over files.
cp -R "${OUT}/exefs/." "${MODS_PATH}/exefs/"
cp -R "${OUT}/romfs/." "${MODS_PATH}/romfs/"

# Indicate completion so VSCode doesn't make us guess
echo "Deployed to Ryujinx: "; date
