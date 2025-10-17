# Sandbox CLI

sandbox_cli provides a real-time, text-based window into the simulation. It drives the runtime API directly, so you can pause, step, and inspect agents without producing offline telemetry first.

## Building

The CLI binary is built together with the rest of the project:

`powershell
cmake -S . -B build
cmake --build build --target genesis_sandbox_cli
`

The executable will be located at uild/src/genesis-sandbox-cli(.exe).

## Running

`powershell
# interactive session
build/src/genesis-sandbox-cli.exe --layout data/ascii_layout.json
`

During an interactive session:

- nter &mdash; advance one simulation step
- step <n> &mdash; advance 
 steps in one batch
- pause / esume &mdash; toggle automatic stepping after each command
- ender &mdash; redraw the current snapshot
- help &mdash; print available commands
- quit / xit &mdash; terminate the CLI

By default the terminal is cleared between frames (ANSI escape sequences). Add --no-clear if your console does not support them or you prefer scrolling output.

### Scripted commands

Provide a semicolon-separated command list to run the CLI non-interactively:

`powershell
build/src/genesis-sandbox-cli.exe 
    --layout data/ascii_layout.json 
    --commands "step 30;pause;step 5;render;quit" 
    --no-clear
`

This mode is useful for CI smoke tests or quick regressions.

## Layout configuration

sandbox_cli uses the same layout definition as the former ASCII viewer. The JSON file describes grid dimensions and node coordinates:

`json
{
  "width": 25,
  "height": 11,
  "nodes": [
    { "id": 1, "label": "Town Center", "x": 12, "y": 5 }
  ]
}
`

If the layout file is missing or malformed, the CLI falls back to a small built-in map that mirrors the demo world.

## Development script

scripts/run_sandbox_cli.ps1 automates configuration, build and launch. It accepts the same flags as the executable (-Steps, -Frames, -Delay, -Rebuild, etc.), then hands control over to the interactive session.

## Roadmap

Planned upgrades include richer inspection commands (agent detail cards, watch lists), runtime breakpoints, and integration with a GUI front-end that consumes the same runtime API.
