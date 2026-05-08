"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const repoRoot = path.resolve(__dirname, "..", "..");
const extensionRoot = path.join(repoRoot, "tools", "vscode-xlang-debug");
const packageJsonPath = path.join(extensionRoot, "package.json");
const extensionPath = path.join(extensionRoot, "extension.js");

function loadExtensionWithMockVscode() {
  const registrations = {
    providers: [],
    factories: [],
  };
  const activeFile = path.join(repoRoot, "release", "examples", "full", "bit_ops_basic.xl");
  const workspaceFolder = {
    uri: { fsPath: repoRoot },
  };
  class DebugAdapterExecutable {
    constructor(command, args) {
      this.command = command;
      this.args = args;
    }
  }
  const vscode = {
    workspace: {
      workspaceFolders: [workspaceFolder],
    },
    window: {
      activeTextEditor: {
        document: {
          uri: { fsPath: activeFile },
        },
      },
    },
    debug: {
      registerDebugConfigurationProvider(type, provider) {
        registrations.providers.push({ type, provider });
        return { dispose() {} };
      },
      registerDebugAdapterDescriptorFactory(type, factory) {
        registrations.factories.push({ type, factory });
        return { dispose() {} };
      },
    },
    DebugAdapterExecutable,
  };
  const module = { exports: {} };
  const source = fs.readFileSync(extensionPath, "utf8");
  const context = vm.createContext({
    __dirname: extensionRoot,
    __filename: extensionPath,
    console,
    exports: module.exports,
    module,
    require(name) {
      if (name === "vscode") {
        return vscode;
      }
      return require(name);
    },
  });
  vm.runInContext(source, context, { filename: extensionPath });
  return { exports: module.exports, registrations, workspaceFolder, activeFile, vscode };
}

function testPackageContributesDebugger() {
  const pkg = JSON.parse(fs.readFileSync(packageJsonPath, "utf8"));
  assert(pkg.main === "./extension.js", "extension main should point at extension.js");
  assert(pkg.activationEvents.includes("onDebug:xlang"), "extension should activate for xlang debug sessions");
  const debuggerContribution = pkg.contributes.debuggers.find(debuggerInfo => debuggerInfo.type === "xlang");
  assert(debuggerContribution, "package should contribute the xlang debugger");
  assert(debuggerContribution.configurationAttributes.launch.required.includes("program"), "launch program should be required");
  assert(debuggerContribution.initialConfigurations[0].xldbg === "${workspaceFolder}/release/xldbg.exe", "default xldbg path should use workspaceFolder");
}

function testExtensionRegistersAdapter() {
  const loaded = loadExtensionWithMockVscode();
  const context = { subscriptions: [] };
  loaded.exports.activate(context);

  assert(loaded.registrations.providers.length === 1, "extension should register one configuration provider");
  assert(loaded.registrations.providers[0].type === "xlang", "configuration provider should use xlang type");
  assert(loaded.registrations.factories.length === 1, "extension should register one adapter descriptor factory");
  assert(loaded.registrations.factories[0].type === "xlang", "adapter factory should use xlang type");
  assert(context.subscriptions.length === 2, "extension should track both disposables");
}

function testConfigurationResolutionAndAdapterCommand() {
  const loaded = loadExtensionWithMockVscode();
  loaded.exports.activate({ subscriptions: [] });
  const provider = loaded.registrations.providers[0].provider;
  const factory = loaded.registrations.factories[0].factory;
  const config = provider.resolveDebugConfiguration(loaded.workspaceFolder, {
    program: "${file}",
    xldbg: "${workspaceFolder}/release/xldbg.exe",
  });

  assert(config.type === "xlang", "resolved config should set type");
  assert(config.request === "launch", "resolved config should set launch request");
  assert(config.name === "Debug XL Program", "resolved config should set a default name");
  assert(config.program === loaded.activeFile, "resolved config should expand ${file}");
  assert(
    path.normalize(config.xldbg) === path.join(repoRoot, "release", "xldbg.exe"),
    "resolved config should expand ${workspaceFolder}"
  );
  assert(config.stopOnEntry === false, "resolved config should default stopOnEntry to false");

  const descriptor = factory.createDebugAdapterDescriptor({
    configuration: config,
    workspaceFolder: loaded.workspaceFolder,
  });
  assert(descriptor instanceof loaded.vscode.DebugAdapterExecutable, "factory should create DebugAdapterExecutable");
  assert(
    path.normalize(descriptor.command) === path.join(repoRoot, "release", "xldbg.exe"),
    "adapter command should point at xldbg.exe"
  );
  assert(Array.isArray(descriptor.args), "adapter args should be an array");
  assert(descriptor.args.length === 1 && descriptor.args[0] === "dap", "adapter args should launch stdio DAP mode");
}

testPackageContributesDebugger();
testExtensionRegistersAdapter();
testConfigurationResolutionAndAdapterCommand();
console.log("[PASS] VS Code extension smoke");
