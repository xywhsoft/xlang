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
  - workspace files use `.xlw`; project files use `.xlp`.
  - `.xlw` loads project references and loose workspace files.
  - `.xlp` loads and saves project metadata, build metadata, source files, extension libraries, resource files and boot code.
  - the workspace pane is rendered from the model, not from hardcoded tree nodes.
  - double-clicking a file/resource node in the workspace pane opens it in the editor.
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
release\xlang_ide.exe --check-workspace-file test\fixtures\workspace\Workspace1.xlw
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
