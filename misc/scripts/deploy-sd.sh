#!/bin/bash
set -e

# Mount drive.
sudo mount ${MOUNT_PATH}
# Copy files to drive.
mkdir -p ${MOUNT_PATH}${SD_OUT}/exefs
mkdir -p ${MOUNT_PATH}${SD_OUT}/romfs
cp -R ${OUT}/exefs/. ${MOUNT_PATH}${SD_OUT}/exefs/
cp -R ${OUT}/romfs/. ${MOUNT_PATH}${SD_OUT}/romfs/
# Unmount drive.
sudo umount ${MOUNT_PATH}
