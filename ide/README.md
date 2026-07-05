# xlang IDE

Native xlang IDE workbench built with XGE + XUI.

Current slice is the minimal skeleton:

- XGE/XUI application lifecycle.
- Menu bar, toolbar and status bar.
- DockPanel workbench.
- Project tree, command tree, editor, help, output, task, debug, tools and properties panels.
- Workspace management model and workspace pane:
  - workspace contains projects.
  - project contains source files, extension libraries and resource files.
  - the workspace pane is rendered from the model, not from hardcoded tree nodes.
  - current default model is `Workspace1 / Project1 / src/main.xl`.
- XUI CodeEdit with xlang language rules:
  - keyword/type/literal/comment/string/operator highlighting.
  - `#doc ... #end` directive highlighting.
  - fold ranges for `func`, `class`, `union`, `get`, `let`, `if`, `for`, `while`, `try`, block comments and doc blocks.
  - fold ranges are rebuilt when editor text changes.

Build:

```bat
build.bat
```

Smoke:

```bat
test.bat
```

Language rule smoke:

```bat
release\xlang_ide.exe --check-language-rules
```

Workspace smoke:

```bat
release\xlang_ide.exe --check-workspace
```

Language:

```bat
release\xlang_ide.exe --lang zh
release\xlang_ide.exe --lang en
release\xlang_ide.exe --lang ru
release\xlang_ide.exe --lang ja
release\xlang_ide.exe --lang fr
release\xlang_ide.exe --lang es
```

Runtime note:

- `xge.dll` is copied from `D:\GIT\auto_test_ide\build\xge.dll`.
- Bundled font is copied once from `D:\GIT\auto_test_ide\font\Consolas-with-Yahei_Nerd_Font.ttf` to `release\res`.
- Font config is `release\option\ide.json`.
- UI font and code editor font default to 18px.
- Headers are currently read from `D:\GIT\xge`.
