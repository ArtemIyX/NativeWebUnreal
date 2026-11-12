# NativeWebTests

`NativeWebTests` centralizes the plugin's automation tests. The module type is
`UncookedOnly`, so it is available to editor/uncooked test sessions and is not
linked into cooked Shipping targets.

## Test groups

| Automation path | Coverage |
| --- | --- |
| `NativeWeb.*` | AffineUI surface lifecycle, QuickJS bridge lifecycle, byte/string messaging, limits, Vue bundle, and view close behavior |
| `NativeWebDispatch.*` | Route-key identity/lifetime, decoder registry, typed handler routing, and metadata |
| `NativeWebWebSocket.*` | Game-thread event ordering, server generations, stop/restart behavior, and client/message routing |

The WebSocket fixture types are private test support. They are not runtime
plugin API.

## Platform and renderer requirements

QuickJS/AffineUI tests are Windows-guarded and require a D3D11 editor session
(launch with `-dx11` when selecting the RHI explicitly). Tests that require the
view renderer report a warning and skip their renderer-specific checks when
the active RHI is not D3D11. Dispatch tests do not require that renderer.

Run a group from the Unreal Automation window or the editor console, for
example:

```text
Automation RunTests NativeWeb
Automation RunTests NativeWebDispatch
Automation RunTests NativeWebWebSocket
```

Build/automation setup is project-specific; this module only supplies the
tests and their required private module dependencies.
