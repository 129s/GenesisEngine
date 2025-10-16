# ASCII Telemetry Viewer

This lightweight player replays simulation telemetry frames in the terminal, using simple ASCII glyphs to visualise world nodes, agent positions and pending actions.

## Build prerequisites

The viewer is built alongside the engine targets. If you have not configured the project yet:

```powershell
cmake -S . -B build
cmake --build build
```

This produces:

- `build/src/genesis-engine.exe` – the simulation executable (with telemetry export flags)
- `build/src/genesis-ascii-viewer.exe` – the ASCII playback tool

## Generating telemetry

Run the engine with the desired step count and telemetry output path:

```powershell
build/src/genesis-engine.exe --steps=360 --telemetry-file build/telemetry.json
```

The resulting JSON file contains tick snapshots including resources, planner decisions, action queues and agent locations, which the viewer consumes.

## Playing back telemetry

Launch the viewer and point it at the telemetry file and layout description:

```powershell
build/src/genesis-ascii-viewer.exe `
    --telemetry build/telemetry.json `
    --layout data/ascii_layout.json `
    --frames 80 `
    --delay 120
```

Key options:

| Flag | Description |
| --- | --- |
| `--telemetry <file>` | Path to the exported JSON from the engine. (Required.) |
| `--layout <file>` | Layout mapping from `LocationId` to grid coordinates. Defaults to `data/ascii_layout.json`. |
| `--frames <count>` | Maximum number of frames to show. Negative (default) plays all available ticks. |
| `--delay <ms>` | Frame delay in milliseconds (default 150 ms). |
| `--no-clear` | Disable ANSI screen clearing and cursor hiding; useful for terminals without VT support. |

During playback the terminal is cleared between frames (unless `--no-clear` is used). Agents are marked with `A`, movement targets with `M`, consumption actions with `C`, and resource nodes with `F`/`D`/`S` depending on type. A textual legend listing agent needs and queued actions is printed beneath each frame.

## One-command demo

Use the bundled script to automate build, export and playback:

```powershell
pwsh scripts/run_ascii_demo.ps1 -Steps 360 -Frames 80 -Delay 120
```

Script parameters let you reuse an existing build (`-Rebuild`, `-Reconfigure`), change the telemetry file name, or adjust playback speed.

## Customising layouts

The viewer reads `data/ascii_layout.json`, which stores grid dimensions and node coordinates:

```json
{
  "width": 25,
  "height": 11,
  "nodes": [
    { "id": 1, "label": "Town Center", "x": 12, "y": 5 },
    { "id": 2, "label": "Tavern", "x": 16, "y": 5 }
  ]
}
```

Add or adjust node entries to match new maps or extend the canvas. The viewer will fall back to `<unknown>` labels or generic positions when an ID is missing.

---

For further automation, consider piping the viewer output into log files or integrating it with your preferred terminal multiplexer for side-by-side comparisons. 更新时间: 2025-10-17。
