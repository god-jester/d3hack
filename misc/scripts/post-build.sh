#!/bin/bash
set -e

OUT_EXEFS=${OUT}/exefs
OUT_ROMFS=${OUT}/romfs/d3gui
OUT_NSO=${OUT_EXEFS}/${BINARY_NAME}
OUT_NPDM=${OUT_EXEFS}/main.npdm

# Clear older build.
rm -rf ${OUT}

# Create out directories.
mkdir -p ${OUT_EXEFS}
mkdir -p ${OUT_ROMFS}

# Copy build into exefs
mv ${NAME}.nso ${OUT_NSO}
mv ${NAME}.npdm ${OUT_NPDM}

# Copy romfs payload (all data/* assets).
if [ -d data ]; then
    cp -R data/. ${OUT_ROMFS}/
fi

# Copy ELF to user path if defined.
if [ ! -z $ELF_EXTRACT ]; then
    cp "$NAME.elf" "$ELF_EXTRACT"
fi
