# Current XGE3/XUI3 Windows IDE engine source

This is the source successor for `release/win_x64/xge.dll` and `ide/third_party/xge_sdk`. The source archive freezes 4,504 files from the actual XGE working tree based on commit `86d0a15b0cb2fb0d17463f3283f6898c941eeaa4`, including the explicitly recorded local changes. Those local changes have not been committed to the upstream XGE repository. The earlier candidate was superseded before installation when additional control/accessibility updates arrived.

Verify `source-manifest.json`, the archive SHA256 and every file against `source-freeze.json`. The archive contains a single `xge3/` prefix and excludes build-object caches and runtime executables. Extract to a new writable directory and run:

```powershell
python -X utf8 -B tools/build/build.py ide build/ide
```

Use GCC 16.2.0 and the recorded normal IDE embedded-XRT profile with `-O2`. The DLL, import library, generated configuration and public headers form one matched set. The production build uses embedded XRT in the separate IDE process. Language programs retain their separately named host-XRT extension images, which this IDE-only update does not replace.

For the GUI, extract the existing `xlang-development-20261105-88bd73dc-r1.zip`, apply the subsequent tools overlay and `ide-startup-fix-20261106` source overlay, then install the newly matched `ide/third_party/xge_sdk` from the accompanying xlang revision. Run the ordinary GUI builder with `-Configuration Release -GuiOnly` and an explicit qualified `-LanguageServerPath`. Current uncommitted IDE3/LSP development edits were excluded from this rebuild.

The accompanying `../../provenance/xge-ide-update-20261106/` reports retain the actual commands, paths and input hashes. Source copies and reports remain outside runtime directories. Normal window rendering tests do not certify every GPU or physical screen-reader. The snapshot enables rebuilding; build paths and PE link timestamps can affect byte identity.
