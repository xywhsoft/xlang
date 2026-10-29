# RapidOCR cold parallel initialization is intermittent

Status: initial package-loader race fixed; short-run cold tests pass. Long-run and native bridge validation remain open.

From `release`, after generating the fixture with `check.ps1 -SkipAot` from `demo6`:

```powershell
1..5 | ForEach-Object { ./xl.exe run extlibs/rapidocr/test/parallel_inference.xl -- --cold }
```

Two independent XLang threads each create an engine, recognize the same fixed PNG and release their own engine. In one observed three-process series, the cold run failed once with `rapidocr shim not found` in one worker and passed twice. Earlier runs without prewarming also intermittently failed with `cannot open OCR image` or `detector inference failed`; the other worker could still succeed. The output changes with scheduling. The same test without `-- --cold` preloads the shim and checks the fixture on the main thread: three consecutive runs and the full `check.ps1 -SkipAot -Stress` gate passed.

The minimal `parallel_probe.xl` isolated a package-loader race: the OpenCV and ONNX wrappers set a global `attempted` flag *before* DLL loading completed, so another thread returned unavailable while the first was still initializing. The wrappers now serialize first load with XRT `xrtOnce`. Before the fix, 3/5 cold inference runs failed; the minimal probe failed in both the OpenCV and ONNX stages. After the fix, the C 16-thread first-load test, 5/5 XLang minimal probes, 5/5 cold real-inference runs and the full `check.ps1 -SkipAot -Stress` gate passed on this machine. The cold probe and cold inference are now explicit `-Stress` cases.

This establishes the identified wrapper race and short-run regression. The ONNX wrapper error buffer is now XRT thread-local; a C two-thread barrier test confirms distinct wrapper errors do not overwrite each other. It does **not** sign off cold parallel inference for release: the OpenCV/ONNX/RapidOCR native bridge sources are not all available on this machine, and long-run model-session concurrency plus native bridge error-state isolation still need verification. Do not modify the xlang mainline from this task.
