# NativeWeb runtime module

`NativeWeb` provides a reusable, local-player-owned view that renders HTML and
CSS through AffineUI and runs JavaScript through QuickJS. It exposes view
lifecycle, viewport display, input forwarding, output texture access, and an
opaque string/byte messaging bridge.

This README covers the `NativeWeb` module only. The plugin's dispatch,
content-HTTP, and WebSocket modules are separate systems and are not started or
configured by a NativeWeb view.

## Requirements and limits

- The current renderer is supported on Win64 with the D3D11 RHI.
- AffineUI currently allows one live surface per process. Creating another
  view while one is active fails with an error; close the first view before
  creating a replacement.
- Create, evaluate, tick, and close views on the game thread. AffineUI surface
  and D3D11 rendering work is performed on the RHI lane.
- The packaged workflow reads files only from the project's `Content`
  directory. It does not load arbitrary URLs or JavaScript modules.
- HTML/CSS relative asset URLs are not generally resolved by the surface.
  Package the JavaScript needed by the view in `ui.js`; place supported font
  files in the package's `fonts` directory.

## Enable the module

Enable the NativeWeb plugin and add the runtime module to the consuming game
module's `Build.cs` dependencies:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "NativeWeb"
});
```

Include the local-player subsystem and view headers where needed:

```cpp
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"
#include "NativeWebView.h"
```

## Packaged view workflow

Create this directory structure in the game project's `Content` folder:

```text
Content/
└── NativeWeb/
    └── Hud/
        ├── ui.css
        ├── ui.js
        └── fonts/
            └── (optional .ttf or .otf files)
```

The subsystem creates a small HTML document containing `<div id="app"></div>`,
loads `ui.css`, and evaluates `ui.js`. No `index.html` is required. The
`ContentRelativeDirectory` argument is relative to `Content`, so use
`NativeWeb/Hud`, without a leading slash or `Content/` prefix.

```cpp
#include "NativeWeb.h"
#include "Subsystems/NativeWebLocalPlayerSubsystem.h"

void UMyHudOwner::StartNativeWeb(ULocalPlayer* LocalPlayer)
{
    if (!LocalPlayer)
    {
        UE_LOG(LogNativeWeb, Error, TEXT("Cannot create the HUD view without a local player."));
        return;
    }

    UNativeWebLocalPlayerSubsystem* NativeWeb =
        LocalPlayer->GetSubsystem<UNativeWebLocalPlayerSubsystem>();
    if (!NativeWeb)
    {
        UE_LOG(LogNativeWebSubsystem, Error, TEXT("NativeWeb subsystem is unavailable."));
        return;
    }

    FNativeWebError Error;
    HudView = NativeWeb->CreatePackagedView(
        FName(TEXT("Hud")), TEXT("NativeWeb/Hud"), Error);
    if (!HudView)
    {
        UE_LOG(LogNativeWebSubsystem, Error, TEXT("View creation failed (%s): %s"),
            *Error.Code.ToString(), *Error.Message);
        return;
    }

    HudView->OnMessage().AddUObject(this, &UMyHudOwner::HandleNativeWebMessage);
    HudView->OnError().AddUObject(this, &UMyHudOwner::HandleNativeWebError);
}
```

Keep the view in a reflected `UPROPERTY` on its UObject owner when that owner
must retain it. The local-player subsystem also retains and manages created
views by name. `FindView` retrieves a view, and `DestroyView` closes and
unregisters it.

## Inline view workflow

Use `CreateView` when the document and stylesheet are already available as
strings or when the view needs custom options:

```cpp
FNativeWebViewOptions Options;
Options.PixelSize = FIntPoint(1280, 720);
Options.DpiScale = 1.0f;
Options.Html = TEXT("<html><body><main id=\"app\"></main></body></html>");
Options.Css = TEXT("html, body { margin: 0; width: 100%; height: 100%; }");
Options.bCreateViewportOverlay = true;
Options.ViewportZOrder = 10;

FNativeWebError Error;
UNativeWebView* View = NativeWeb->CreateView(FName(TEXT("Menu")), Options, Error);
```

`FNativeWebViewOptions` also contains `ClearColor` and an optional
`PackagedAssetRoot`. The latter is a project-Content-relative directory used
for packaged document assets and its `fonts` subdirectory. To load a document
after creation, call `LoadPackagedDocument` with normalized paths relative to
that root. To execute a separate script, call `EvaluatePackagedScript`.

## View lifecycle and presentation

- `CreatePackagedView` and `CreateView` create the view and register it with
  the owning local-player subsystem. Creation fails for an empty/duplicate
  name, invalid content path/options, unsupported renderer, or occupied
  single-surface capacity.
- `Show` makes the view visible and attaches it to the local-player viewport
  when needed. `Hide` hides it while keeping its runtime alive.
- `AttachToPlayerViewport` and `DetachFromPlayerViewport` manage the Slate
  overlay independently of view creation. The view's Slate widget forwards
  pointer, wheel, keyboard, and focus events while attached.
- `ForwardInput` lets a custom host forward an `FNativeWebInputEvent` directly.
  Pointer positions use normalized coordinates; the supported key enum is
  limited to Tab, Enter, and Space.
- `GetTexture` returns the view-owned `UTextureRenderTarget2D` for custom
  presentation. Do not destroy it or access its RHI resource from arbitrary
  threads.
- `Close` is idempotent. It stops message delivery, detaches the widget,
  releases view-owned focus/capture, invalidates callbacks, and tears down the
  runtime. `DestroyView` additionally removes the view from the subsystem map.

The Slate view manages its own focus and pointer capture. It does not replace
or silently modify the `PlayerController` input mode or input mappings.

## Two-way messaging

The JavaScript bridge is available as `globalThis.nativeWeb`.

```javascript
const unsubscribe = nativeWeb.addMessageListener((message) => {
  if (message.type === "string") {
    console.log("Native message", message.channel, message.value);
  } else if (message.type === "bytes") {
    // value is a Uint8Array containing an owned copy of the payload.
    console.log("Native byte count", message.value.length);
  }
});

nativeWeb.postString("ready", "status");
nativeWeb.postBytes(new Uint8Array([0x01, 0x02, 0x03]), "binary");

// Call when this JavaScript listener is no longer needed.
unsubscribe();
```

`postString(value, channel?)` accepts a string and an optional string channel.
`postBytes(value, channel?)` accepts an `ArrayBuffer` or an ArrayBuffer view and
an optional string channel. Both return `true` when queued and `false` when the
view is closing or the bounded queue cannot accept the message. Invalid
argument types throw a JavaScript `TypeError`; an individual payload above
1 MiB throws a `RangeError`.

An Unreal-to-JavaScript listener receives an object with this shape:

```javascript
{
  channel: "status",        // omitted when no channel was supplied
  type: "string",           // "string" or "bytes"
  value: "ready"             // string, or Uint8Array for bytes
}
```

JavaScript-to-Unreal messages are copied into an owned queue by the host
callback and delivered to `UNativeWebView::OnMessage()` after the QuickJS call
has unwound. Unreal-to-JavaScript messages are copied into a bounded queue and
delivered during the subsystem's game-thread tick; the bridge also pumps
QuickJS jobs with a per-tick budget. Neither direction invokes application
handlers reentrantly from a JavaScript host callback.

The bridge also provides DOM helpers for the embedded AffineUI surface,
including `mountRoot`, `createElement`, `createText`, `createComment`,
`insertBefore`, `remove`, `setNodeText`, `setAttribute`, `removeAttribute`,
and `setClickHandler`. `setText(id, text)` queues a text update for an existing
element ID. These APIs are constrained by the DOM operations AffineUI exposes;
they are not a browser DOM implementation.

In C++, `OnMessage()` broadcasts a `const FNativeWebMessage&`. Its `Channel` is
an `FName`, `Type` selects `String` or `Bytes`, and the payload remains opaque
to NativeWeb. No JSON/protobuf format or application schema is imposed. Bind
with `AddUObject` for UObject listeners, or retain and remove the delegate
handle when binding other callable types. `OnError()` broadcasts
`FNativeWebError` details for creation, evaluation, rendering, or message
delivery errors.

Each direction has bounded queue capacity: up to 64 queued messages, 4 MiB of
queued payload, and 1 MiB per message. Check the return value from
`EnqueueStringToJavaScript` or `EnqueueBytesToJavaScript` to distinguish
`RESULT_QUEUED`, `RESULT_CLOSED`, and `RESULT_QUEUE_FULL`. The convenience
`SendStringToJavaScript` and `SendBytesToJavaScript` methods return whether the
message was accepted, not whether JavaScript processed it.

## Blueprint use

Blueprint callers should obtain `UNativeWebLocalPlayerSubsystem` from the
owning local player and call `Create Packaged View` or `Create View`. Keep the
returned `Native Web View` reference while it is needed, then use `Show`,
`Hide`, and `Destroy View` for presentation and cleanup. The packaged workflow
uses the same `Content/<directory>/ui.css` and `ui.js` convention described
above. The message and error delegates are native C++ multicast delegates and
are not Blueprint-assignable in this module version.

## Logging

Include `NativeWeb.h` to use the module's log categories. Categories are split
by subsystem, view lifecycle, assets, scripts, messaging, input, viewport,
AffineUI, rendering, and ticking. Set a category to `Verbose` or
`VeryVerbose` when tracing startup or message delivery; payload contents are
not logged by the bridge.

## Related documentation

- The [plugin README](../../README.md) covers all modules and their
  relationships.
- The `NativeWebDispatch`, `NativeWebContentHttpServer`, and
  `NativeWebWebSocketServer` READMEs cover their independent module APIs.
