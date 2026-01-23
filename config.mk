#----------------------------- User configuration -----------------------------

# Common settings
#------------------------

# How you're loading your module. Used to determine how to find the target module. (AsRtld/Module/Kip)
LOAD_KIND := Module

# Program you're targeting. Used to determine where to deploy your files.
PROGRAM_ID := 01001b300b9be000

# Optional path to copy the final ELF to, for convenience.
ELF_EXTRACT :=

# Python command to use. Must be Python 3.4+.
PYTHON := python3

# JSON to use to make .npdm
NPDM_JSON := qlaunch.json

CONFIG_JSON := d3hack-nx.json

# Additional C/C++ flags to use.
C_FLAGS := 
CXX_FLAGS := 

# AsRtld settings
#------------------------

# Path to the SD card. Used to mount and deploy files on SD, likely with hekate UMS.
MOUNT_PATH := /mnt/k

# Module settings
#------------------------

# Settings for deploying over FTP. Used by the deploy-ftp.py script.
FTP_IP := 10.0.30.77
FTP_PORT := 5000
FTP_USERNAME := anonymous
FTP_PASSWORD :=

# Settings for deploying to Ryu. Used by the deploy-ryu.sh script.
RYU_WIN_PATH := /home/jester/AppData/Roaming/Ryujinx/
RYU_MAC_PATH := /Users/dev/Library/Application Support/Ryujinx/

# Settings for deploying to Yuzu. Used by the deploy-yuzu.sh script.
YUZU_PATH := /home/jester/AppData/Roaming/yuzu/load/
