# QuickJS and AffineUI bridge: current API boundary

## Current ownership

`FQuickJSRuntime` owns one QuickJS runtime and context. Create, evaluate, pump pending jobs, close, and destroy it on the game thread. Its destructor also requires the game thread; owned `JSValue`s must be freed before closing the context.

`FAffineUiSurface` is game-thread-owned. It takes HTML/CSS snapshots and queues work through `FAffineUiSurfaceRenderer`. The renderer creates and retains the opaque `affineui_ui` and its D3D11 resources on the RHI lane. Rendering and destroying that UI happen on that lane; completion callbacks return to the game thread. The surface currently has a single-active-instance process restriction. `FAffineUiScriptInstance` owns a QuickJS runtime and one surface; it creates, evaluates, pumps jobs, and tears them down on the game thread.

## Available document mutations

The bundled `AffineUnreal/Source/AffineUnreal/Public/affineui.h` C++ `Document` API supports live `set_text_by_id`, `set_attribute_by_id`, `remove_attribute_by_id`, and weak DOM handles from `weak_handle_for_id` with `weak_handle_valid`. These can mutate existing parsed nodes. The public C document API also declares text and attribute mutation operations.

The public API does not expose incremental creation or insertion of DOM nodes. `Document::set_html` replaces the document markup. Replacing the HTML snapshot after each JS update would reparse the document and cannot serve as a substitute for creation/insertion operations. The embedded C API does expose `affineui_ui_set_text(ui, id, text)`, so the bridge supports text updates to an existing element selected by ID. IDs are not generation-counted node handles and can resolve differently after replacing the markup.

## Integration boundary and required extension

The present NativeWeb renderer uses `affineui_ui_create`, `affineui_ui_set_html`, and `affineui_ui_render`; it does not retain an exposed `Document` handle. QuickJS must not call AffineUI from a JS callback because that callback runs on the game thread while AffineUI's live UI is owned by the RHI lane.

A safe text-update slice uses the surface's copied text-update map as a command queue. `nativeWeb.setText(id, text)` converts JS strings and queues owned Unreal strings on the game thread; its `true` return means the update was queued, not confirmed rendered. The existing render scheduling sends a copied snapshot to the RHI lane, where `affineui_ui_set_text` runs before drawing. The renderer caches HTML and CSS and only calls their setters when those snapshots change, so a text update does not reset/reparse the document. No QuickJS values, context pointers, or borrowed JS strings cross threads. The embedded `affineui_ui_set_text` result only reports whether content changed; it cannot distinguish an invalid ID from an unchanged value. Thus the bridge rejects malformed/empty IDs but cannot synchronously report stale/unknown element IDs.

A complete node-building bridge additionally requires AffineUI to expose incremental create and insert/append operations (and a way to retain and validate stable node handles through the embedded `affineui_ui` API). Until then, document construction has to be supplied in the initial HTML snapshot. The smallest AffineUI extension is a pair of owner-thread operations to create a supported node and insert it under a parent/before a sibling, returning a weak handle whose validity can be checked after removals. These operations are intentionally not emulated by replacing the HTML snapshot.

## Verification plan

The standalone fixture is `Resources/Demo/quickjs_affine_text.js`; its host must already have an initial HTML node with ID `bridge-status`. Build the Win64 editor target with D3D11 enabled, then run `NativeWeb.AffineUi.QuickJS.BridgeLifecycleAndTextUpdates` and the existing `NativeWeb.AffineUi.Surface.Lifecycle` automation test. The bridge test covers two text updates against the same existing element ID, malformed input, JavaScript exceptions, a deferred promise job, repeated close, and reinitialization. Unknown-ID validation and create/insert identity tests remain unsupported by the current embedded AffineUI API. Do not run these tests on a non-D3D11 RHI: the existing surface rejects it by design.
