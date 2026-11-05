# Static import-kind source correction

Apply this small source overlay after ../xge-static-20261106-r1. It does not
repeat the Native source archive or recompile its 145 native objects.

A real AMD64 jmp-through-IAT relocation proves a function thunk. IAT-only
entries are resolved by bounded readonly System32 PE export/section and
forwarder analysis. Unknown remains unknown; static staging rejects it.
The current full inventory has zero unknown kinds. Exact DLL/member/code
hashes and the corrected 36 formerly misclassified functions are recorded
by the external build and test receipts named in manifest.json. No DLL is
loaded for classification, no Native ABI changes, no release installation.

Run Native tools/test/check_static.py with the normal root/build/xlang-root/
out arguments plus --import-kinds-only for the readonly real-PE proof.
The final metadata build is win-default-r12; the current prepared package
is packages-win-r5. PE/AOT execution qualification is separate.
