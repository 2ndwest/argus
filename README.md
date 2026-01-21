# argus

esp32-based door sensor that reports state changes via HTTP webhook

## Setup

Copy the example config file:

```bash
cp main/config.h.example main/config.h
```

Then edit `main/config.h` with your settings:

- `WIFI_SSID` - Your WiFi network name
- `WIFI_PASS` - Your WiFi password
- `STATE_CHANGE_URL` - Webhook endpoint URL for state change notifications
- `WEBHOOK_SECRET` - Secret token sent in `x-webhook-secret` header
- `DOOR_ID` - Identifier for this door sensor

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py flash
```

### Monitor

```bash
idf.py monitor
```

### Clangd (VS Code)

1. **Build the project first** - clangd needs `build/compile_commands.json`:
   ```bash
   idf.py build
   ```

2. **Configure `.vscode/settings.json`** with the correct toolchain paths:
   ```json
   {
       "clangd.path": "~/.espressif/tools/esp-clang/<VERSION>/esp-clang/bin/clangd",
       "clangd.arguments": [
           "--query-driver=~/.espressif/tools/xtensa-esp-elf/<VERSION>/xtensa-esp-elf/bin/xtensa-esp32-elf-*"
       ]
   }
   ```

   **Important:** Match `--query-driver` to your target chip:
   - ESP32 (Xtensa): `xtensa-esp32-elf-*`
   - ESP32-S2/S3 (Xtensa): `xtensa-esp32s2-elf-*` / `xtensa-esp32s3-elf-*`
   - ESP32-C3/C6/H2 (RISC-V): `riscv32-esp-elf-*`

3. **Find your installed versions:**
   ```bash
   ls ~/.espressif/tools/esp-clang/
   ls ~/.espressif/tools/xtensa-esp-elf/   # or riscv32-esp-elf for RISC-V chips
   ```

4. **Restart clangd** after changes: `Cmd+Shift+P` → "clangd: Restart language server"
