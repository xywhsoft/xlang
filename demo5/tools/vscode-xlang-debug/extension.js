"use strict";

const path = require("path");
const vscode = require("vscode");

function workspacePath(folder) {
  if (folder && folder.uri && folder.uri.fsPath) {
    return folder.uri.fsPath;
  }
  const first = vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0];
  return first && first.uri ? first.uri.fsPath : "";
}

function activeFilePath() {
  const editor = vscode.window.activeTextEditor;
  return editor && editor.document && editor.document.uri ? editor.document.uri.fsPath : "";
}

function resolveVars(value, folder) {
  if (typeof value !== "string") {
    return value;
  }
  const root = workspacePath(folder);
  const file = activeFilePath();
  return value
    .replace(/\$\{workspaceFolder\}/g, root)
    .replace(/\$\{file\}/g, file);
}

function defaultXldbg(folder) {
  const root = workspacePath(folder);
  return root ? path.join(root, "release", "xldbg.exe") : "xldbg.exe";
}

class XLangDebugConfigurationProvider {
  resolveDebugConfiguration(folder, config) {
    if (!config.type) {
      config.type = "xlang";
    }
    if (!config.request) {
      config.request = "launch";
    }
    if (!config.name) {
      config.name = "Debug XL Program";
    }
    if (!config.program) {
      config.program = activeFilePath();
    }
    if (!config.xldbg) {
      config.xldbg = defaultXldbg(folder);
    }
    config.program = resolveVars(config.program, folder);
    config.xldbg = resolveVars(config.xldbg, folder);
    config.stopOnEntry = Boolean(config.stopOnEntry);
    return config;
  }
}

class XLangDebugAdapterDescriptorFactory {
  createDebugAdapterDescriptor(session) {
    const xldbg = resolveVars(session.configuration.xldbg || defaultXldbg(session.workspaceFolder), session.workspaceFolder);
    return new vscode.DebugAdapterExecutable(xldbg, ["dap"]);
  }
}

function activate(context) {
  context.subscriptions.push(
    vscode.debug.registerDebugConfigurationProvider("xlang", new XLangDebugConfigurationProvider()),
    vscode.debug.registerDebugAdapterDescriptorFactory("xlang", new XLangDebugAdapterDescriptorFactory())
  );
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
