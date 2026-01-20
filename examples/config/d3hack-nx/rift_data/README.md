# Rift Data Layout

Place weekly Challenge Rift files here so the mod can emulate online content while offline. Files are read from `sd:/config/d3hack-nx/rift_data` at runtime.

Required files
- `challengerift_config.dat` — Weekly configuration protobuf blob.
- `challengerift_00.dat` … `challengerift_99.dat` — Weekly rift data blobs (two-digit naming). Add as many as you want and control the range in config.

Tips
- You can capture real files once (online) using the debug dumper and then reuse them offline. See `tools/import_challenge_dumps.py` to import dumps into this folder.
- The mod can choose a specific file or a pseudo-random one in a range; adjust as desired.
