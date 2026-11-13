// Evaluated by FAffineUiScriptInstance after the initial HTML snapshot is rendered.
// A later call to update() mutates the same existing AffineUI element by id.
globalThis.quickJsAffineTextFixture = {
	show() { nativeWeb.setText("bridge-status", "QuickJS is connected to AffineUI"); },
	update() { nativeWeb.setText("bridge-status", "Text updated without replacing the document"); }
};
quickJsAffineTextFixture.show();
