# Suggested Commands

## Environment Setup
```bash
source ~/esp/v5.5.3/esp-idf/export.sh
```

## Build Commands (run from float_core/, float_node/, or float_zigbee/)
```bash
idf.py set-target <chip>       # esp32s3, esp32c6
idf.py build
idf.py flash monitor
idf.py build flash monitor     # combined
```

## Generate compile_commands.json for clangd
```bash
idf.py reconfigure
ln -sf build/compile_commands.json compile_commands.json
```

## System Utils
- `git` — version control
- `ls`, `find`, `grep` — standard Linux utils
