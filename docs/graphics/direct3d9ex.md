# Direct3D 9Ex Runtime Toggle

The legacy Star Wars Galaxies client now exposes a runtime toggle for the Direct3D 9Ex renderer. 9Ex improves windowed-mode presentation and reduces compatibility issues on modern Windows releases while staying compatible with the 32-bit client.

## Configuration Keys

Add the following entries to the `Direct3d9` section of your configuration (for example `clientdata/Direct3d9.cfg`) to control the renderer behaviour:

| Key | Default | Description |
| --- | --- | --- |
| `Direct3d9.preferDirect3d9Ex` | `true` | Attempts to load the Direct3D 9Ex runtime via `Direct3DCreate9Ex`. When unavailable the client falls back to the classic Direct3D 9 path and records the reason in the crash report log. |
| `Direct3d9.maximumFrameLatency` | `1` | Sets `IDirect3DDevice9Ex::SetMaximumFrameLatency` on successful 9Ex device creation. Values outside `1-16` are clamped to preserve stability on lower-end 32-bit hardware. |

> ℹ️ The options above are ignored when the system does not provide the Direct3D 9Ex entry points. The client records a `Direct3D9ExFallback` entry in the crash report to aid support triage.

## Behaviour Notes

* The 9Ex path is dynamically loaded, so the 32-bit binary remains compatible with systems that only ship `d3d9.dll`.
* Frame latency is only configured when a 9Ex device is active. The classic Direct3D 9 runtime does not expose the API and remains unchanged.
* Verbose hardware logging (`SharedFoundation` → `verboseHardwareLogging`) emits additional details about the selected runtime and any fallback decisions.

