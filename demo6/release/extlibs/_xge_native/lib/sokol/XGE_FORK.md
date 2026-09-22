# XGE Sokol Fork

XGE owns the vendored Sokol integration in this directory.

The Win32 message loop exposes `SOKOL_WIN32_PRETRANSLATE_MESSAGE` before
`TranslateMessage`. XGE uses this hook to let TSF consume keyboard messages
before Windows can synthesize `WM_CHAR`. Keep this hook when synchronizing
with upstream Sokol.
