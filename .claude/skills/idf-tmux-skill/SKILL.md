---
name: idf-tmux-skill
description: >
  Use this skill when working with ESP-IDF projects that require flashing firmware or monitoring serial output via idf.py. Triggers when the user wants to flash an ESP32 device, read monitor output, watch for boot messages, detect crashes or panics, or interact with idf.py monitor in any way. Also use when the user wants Claude to react to serial output content — e.g. "flash and check if it boots", "monitor and tell me when it crashes", "run the monitor and read the logs". Since idf.py monitor requires a real TTY, this skill uses tmux to drive it and capture output.
---

# IDF tmux Skill

Drive `idf.py flash` and `idf.py monitor` from Claude Code using tmux as a PTY wrapper. Enables Claude to flash firmware, read live serial output, and react to log content.

## Prerequisites

- `tmux` installed (`apt install tmux` / `brew install tmux`)
- ESP-IDF environment sourced (or idf.py on PATH)
- Device connected and port accessible

## Workflow

### 1. Start or reuse tmux session

```bash
# Check if session exists
tmux has-session -t idf 2>/dev/null || tmux new-session -d -s idf -x 220 -y 50
```

Always reuse the `idf` session if it exists. This avoids orphaned monitor processes.

### 2. Exit any running monitor first

If monitor is already running from a previous invocation, exit cleanly before flashing:

```bash
tmux send-keys -t idf C-t C-x
sleep 2
```

### 3. Flash

```bash
tmux send-keys -t idf "idf.py flash" Enter
```

Poll until flash completes:

```bash
python3 /path/to/scripts/poll_pane.py idf "Hard resetting\|Leaving\.\.\.\|error" 60
```

Check the returned output for errors before proceeding.

### 4. Start monitor

```bash
tmux send-keys -t idf "idf.py monitor" Enter
sleep 2  # let monitor initialize
```

### 5. Read output

Use the helper script to capture a snapshot:

```bash
python3 scripts/capture_pane.py idf 500
```

This returns the last N lines from the pane. Call it repeatedly to get rolling output. For continuous watching, use `poll_pane.py` with a pattern.

### 6. React to content

Analyze the captured output directly — look for:
- `Guru Meditation Error` / `panic` → crash, report the backtrace
- `ESP_ERROR_CHECK failed` → runtime assertion with file/line info
- `rst:0xc (RTC_SW_CPU_RST)` → software reset (crash reboot loop)
- `Restarting...` → watchdog or intentional reset
- Custom markers like `TEST_PASS`, `TEST_FAIL`, app-specific logs
- Successful boot indicators (app-defined)

### 7. Exit monitor when done

```bash
tmux send-keys -t idf C-t C-x
sleep 2
```

---

## Monitor Keyboard Shortcuts (Ctrl-T menu)

Inside `idf.py monitor`, `Ctrl-T` is the menu key. Useful sequences:

| Keys | Action |
|------|--------|
| `Ctrl-T Ctrl-X` | **Exit monitor cleanly** (preferred over Ctrl-]) |
| `Ctrl-T Ctrl-R` | **Reboot device** (software reset via RTS) |
| `Ctrl-T Ctrl-H` | Show help / all available shortcuts |
| `Ctrl-T Ctrl-F` | Enter boot rom download mode |

When sending these via tmux:
```bash
tmux send-keys -t idf C-t C-x    # exit monitor
tmux send-keys -t idf C-t C-r    # reboot device
```

---

## Using the REPL

If the firmware includes a console REPL (e.g. via `esp_console`), you can send commands directly through the monitor:

```bash
# Send a command
tmux send-keys -t idf "help" Enter
sleep 1
python3 scripts/capture_pane.py idf 30

# Send a reboot command via REPL
tmux send-keys -t idf "restart" Enter
```

**Important**: Only send REPL commands when the monitor is running and the REPL prompt is visible. Check with `capture_pane.py` first.

---

## Helper Scripts

See `scripts/` — read them before use if behavior needs adjusting.

- `capture_pane.py` — snapshot last N lines from a tmux pane
- `poll_pane.py` — wait for a regex pattern to appear, with timeout

---

## Do's

- **Always check the pane state** before sending commands — is it at a shell prompt, mid-flash, or in the monitor?
- **Use `poll_pane.py`** to wait for flash/boot completion rather than arbitrary sleeps
- **Use `idf.py build flash monitor`** as a single command when you want build+flash+monitor — idf.py handles the sequencing
- **Wait for the REPL prompt** before sending REPL commands
- **Delete `sdkconfig`** when changing `sdkconfig.defaults` — `idf.py fullclean` does NOT delete it, and stale sdkconfig values override defaults
- **Use `Ctrl-T Ctrl-X`** to exit monitor cleanly
- **Use `Ctrl-T Ctrl-R`** to reboot the device without exiting the monitor
- **Poll for new content** after the flash+monitor cycle completes — the pane buffer may contain output from previous boot cycles

## Don'ts

- **Don't send `Ctrl-C`** to cancel a flash mid-write — this corrupts the firmware image and bricks until reflashed
- **Don't send REPL commands while flash is in progress** — keystrokes go to the serial port and interfere with flashing
- **Don't rely on the first `poll_pane.py` match** after a rebuild — the pane buffer still contains output from previous boots. Poll for patterns unique to the NEW boot (e.g. a new ELF SHA256, or wait for the monitor startup line first)
- **Don't use `Ctrl-]`** to exit monitor — `Ctrl-T Ctrl-X` is the proper exit sequence
- **Don't assume `fullclean` gives a fresh config** — you must also `rm sdkconfig` for `sdkconfig.defaults` changes to take effect

---

## Notes

- `idf.py flash` and `idf.py monitor` can be chained: `idf.py flash monitor` — but splitting them gives better control over flash error handling.
- If the port is wrong, pass `-p /dev/ttyUSB0` (or relevant port) explicitly.
- If IDF env isn't sourced, prepend `. $IDF_PATH/export.sh &&` to commands.
- tmux `capture-pane` returns a plain-text snapshot — ANSI escape codes are stripped by default with `-e` flag omitted.
- When `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` is set, ALL output (bootloader + app) goes to the USB JTAG port, not UART. If monitoring via `/dev/ttyUSB0` (UART), you won't see any boot messages. Use `CONFIG_ESP_CONSOLE_UART_DEFAULT=y` to ensure output on UART.
