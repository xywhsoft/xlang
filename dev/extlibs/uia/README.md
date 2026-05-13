# x.uia Windows UI Automation extension

Status: V1 closed; V2.1-V2.8 implemented
Design directory: `dev/extlibs/uia`
Implementation package directory: `demo5/release/libs/x.uia`
Target platform: Windows first
Target package: `x.uia`
High-level package: `x.ui.auto`

This directory records the design and delivery plan for the official xlang
Windows UI Automation extension. The extension code itself must be placed under
`demo5/release/libs/x.uia`, matching the current release package layout.

## Documents

- [x.uia-v1-spec.md](x.uia-v1-spec.md): package boundary, API, native bridge,
  implementation details, examples, validation rules, and risks.
- [x.uia-implementation-tracker.md](x.uia-implementation-tracker.md): phased
  task tracker with acceptance criteria.
- [x.uia-smoke.md](x.uia-smoke.md): interactive smoke commands and test
  separation notes.

## Implementation Rule

`dev/extlibs/uia` is only the design and tracking area. Do not place the live
plugin implementation here.

The live package must be created as:

```text
demo5/release/libs/x.uia/
  package.json
  src/uia.xl
  src/x_uia_artifact.c
  src/x_uia_min.h        # local minimal UIA COM declarations for release builds
```

Examples and smoke tests should be added under:

```text
demo5/release/examples/xle_official_native/
```

The focused UIA smoke runner is:

```bat
dev\extlibs\uia\run_x_uia_static_smoke.bat
dev\extlibs\uia\run_x_uia_smoke.bat
```

## Design Position

`x.uia` is a native Windows accessibility automation package. It should not be
implemented as a generic `x.com` wrapper because UI Automation is centered on
strong COM interfaces such as `IUIAutomation`, `IUIAutomationElement`,
`IUIAutomationCondition`, and pattern interfaces. The package should reuse the
same XLE source-mixed style proven by `x.com`, but its native side should call
UIA COM interfaces directly.
