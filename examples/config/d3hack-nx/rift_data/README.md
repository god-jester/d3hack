# Rift Data Layout

Place weekly Challenge Rift files here so the mod can emulate online content while offline. Files are read from `scratch:/config/d3hack-nx/rift_data` at runtime (maps to `sd:/config/d3hack-nx/rift_data` on hardware).

Required files
- `challengerift_config.dat` — Weekly configuration protobuf blob.
- `challengerift_0.dat` … `challengerift_9.dat` — Weekly rift data blobs. You can add more up to `challengerift_19.dat`.

Tips
- You can capture real files once (online) using the debug dumper and then reuse them offline. See `tools/import_challenge_dumps.py` to import dumps into this folder.
- The mod can choose a specific file or a pseudo-random one in a range; adjust as desired.

