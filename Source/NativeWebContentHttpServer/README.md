# NativeWebContentHttpServer

`NativeWebContentHttpServer` provides a game-instance subsystem for serving
image responses over HTTP and retrieving/caching textures by package path. It
is independent of the NativeWeb QuickJS/AffineUI view runtime.

## Subsystem

Get `UNativeWebContentHttpServerSubsystem` from the game instance. The
subsystem starts its configured listener during initialization and stops it
during deinitialization. `StartServer` can override the configured port;
`StopServer` and `IsServerRunning` control/query the listener. The default
image route is `/img`.

`GetImageByPackagePath` returns a `UTexture2D` and a success flag. The image
cache is owned by the game-instance service. Use `ClearCachedImages` to evict
entries or `GetCachedImageCount` to inspect the current cache size.

## Request handlers

`UNativeWebContentHttpImageRequestHandler` is the pluggable handler base. A
handler receives `FNativeWebContentHttpImageRequestContext` and produces an
`FNativeWebContentHttpImageResponse` containing a status code, content type,
and byte body. The subsystem owns the handler instance. Set a valid concrete
handler class through `SetRequestHandlerClass`. Passing null fails handler
creation; the default class is selected during subsystem initialization.

`UNativeWebContentDefaultImageRequestHandler` is the default implementation.
Override the handler contract when the project needs a different asset lookup,
authorization rule, response format, or asynchronous image-processing path.

## Lifetime and dependencies

The subsystem owns the HTTP service, handler UObject, and cache for one game
instance. It is a separate runtime module from `NativeWeb`,
`NativeWebDispatch`, and `NativeWebWebSocketServer`; none of those services
starts automatically when this module is used.

See [the plugin README](../../README.md) for the full module map and
module-specific documentation links.
