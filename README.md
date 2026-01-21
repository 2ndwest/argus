# argus

esp32-based door sensor that reports state changes via HTTP webhook

## build setup

1. Copy the example config file:
   ```bash
   cp main/config.h.example main/config.h
   ```

2. Edit `main/config.h` with your wifi credentials, etc.
3. **Build:** `idf.py build`
4. **Flash:** `idf.py flash`
5. **Monitor:** `idf.py monitor`

## clangd setup

1. Build the project first, clangd needs `build/compile_commands.json`:
   ```bash
   idf.py build
   ```

2. Configure clangd with the correct toolchain paths, e.g. in `.vscode/settings.json`:
   ```json
   {
       "clangd.path": "/Users/<USERNAME>/.espressif/tools/esp-clang/<VERSION>/esp-clang/bin/clangd",
       "clangd.arguments": [
           "--query-driver=/Users/<USERNAME>/.espressif/tools/xtensa-esp-elf/<VERSION>/xtensa-esp-elf/bin/xtensa-esp32-elf-*"
       ]
   }
   ```

   **Important:** Match `--query-driver` to your target chip:
   - ESP32 (Xtensa): `xtensa-esp32-elf-*`
   - ESP32-S2/S3 (Xtensa): `xtensa-esp32s2-elf-*` / `xtensa-esp32s3-elf-*`
   - ESP32-C3/C6/H2 (RISC-V): `riscv32-esp-elf-*`

3. Find your installed versions:
   ```bash
   ls ~/.espressif/tools/esp-clang/
   ls ~/.espressif/tools/xtensa-esp-elf/  # or riscv32-esp-elf for RISC-V chips
   ```

4. Restart clangd after changes: `Cmd+Shift+P` → "clangd: Restart language server"
