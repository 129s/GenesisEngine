# Sandbox CLI

`sandbox_cli` provides a real-time, text-based window into the simulation. It drives the runtime API directly, letting you pause, step, and inspect agents without producing offline telemetry first.

## Building

```powershell
cmake -S . -B build
cmake --build build --target genesis_sandbox_cli
```

The executable is emitted to `build/src/genesis-sandbox-cli(.exe)`.

## Running

```powershell
# interactive session
build/src/genesis-sandbox-cli.exe --layout data/ascii_layout.json
```

During an interactive session:

- `Enter` — advance one simulation step
- `step <n>` — advance `n` steps (rendered one frame at a time)
- `pause` / `resume` — toggle automatic stepping after each command
- `render` — redraw the current snapshot
- `help` — print available commands
- `quit` / `exit` — terminate the CLI

By default the terminal is cleared between frames (ANSI escape sequences). Add `--no-clear` if your console does not support them or you prefer scrolling output.

### Frame pacing

The CLI throttles scripted playback to 60 fps by default. Use `--fps <value>` to pick a different target (set to `0` to disable) or `--frame-time-ms <n>` to supply an exact minimum frame time. Interactive input remains instantaneous, so pressing `Enter` still refreshes immediately.

### Scripted commands

Provide a semicolon-separated command list to run the CLI non-interactively:

```powershell
build/src/genesis-sandbox-cli.exe `
    --layout data/ascii_layout.json `
    --commands "step 30;pause;step 5;render;quit" `
    --fps 60
```

This mode is useful for CI smoke tests or quick regressions. When throttling is enabled, each scripted `step` command honours the configured frame pacing, which avoids flicker and keeps recordings consistent.

## Layout configuration

`sandbox_cli` uses the same layout definition as the former ASCII viewer. The JSON file describes grid dimensions and node coordinates:

```json
{
  "width": 25,
  "height": 11,
  "nodes": [
    { "id": 1, "label": "Town Center", "x": 12, "y": 5 }
  ]
}
```

If the layout file is missing or malformed, the CLI falls back to a small built-in map that mirrors the demo world.

## Development script

`scripts/run_sandbox_cli.ps1` automates configuration, build, and launch. Key flags:

- `-Steps <n>` — scripted step count before exiting (default 120)
- `-Fps <n>` — target render fps forwarded to `--fps` (`0` disables limiting)
- `-NoClear` — forward `--no-clear` to the CLI
- `-Interactive` — skip scripted commands and drop straight into the prompt

Unless `-Interactive` is supplied, the script runs the CLI with the requested scripted commands, then exits when they finish.


## Graph-to-grid mapping

CLI 渲染不会自动“从图生成几何布局”。它基于运行时快照中的 `LocationId`，借助布局 JSON 的 `id → (x,y)` 映射，将节点投影到字符网格：
- 资源 → `'F'`（Food）/`'R'`（其他）
- 行动 → `'C'`（ConsumeResource）/`'M'`（其他）
- 代理 → `'A'`

详见：`docs/architecture/GRAPH_TO_GRID.md`
## Roadmap

Planned upgrades include richer inspection commands (agent detail cards, watch lists), runtime breakpoints, and integration with a GUI front-end that consumes the same runtime API.

See also: `../roadmap/README.md` for the latest unified roadmap and milestones.
