"""Stage the native browser dependency and its local renderer assets."""
from pathlib import Path
import hashlib
import shutil


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def engine_runtime_inputs(metadata, inputs):
    dependencies = {}
    for row in metadata.get("runtime_dependencies", []):
        path = Path(row["path"])
        if sha(path) != row["sha256"]:
            raise ValueError(f"Engine runtime dependency changed: {path}")
        if path.name.casefold() in dependencies:
            raise ValueError(f"Duplicate engine runtime dependency: {path.name}")
        inputs[str(path)] = row["sha256"]
        dependencies[path.name.casefold()] = path
    return dependencies


def browser_inputs(metadata, engine, dependencies, include_assets, inputs):
    loader = dependencies.get("webview2loader.dll")
    if loader is None:
        return None

    row = next(row for row in metadata["runtime_dependencies"]
               if Path(row["path"]) == loader)
    source = Path(row.get("source", row["path"]))
    if source.parent.parts[-3:] != ("build", "native", "x64") or sha(source) != row["sha256"]:
        raise ValueError("The WebView2 build report must identify the exact SDK Loader")
    inputs[str(source)] = row["sha256"]

    sdk = source.parents[3]
    notices = (sdk / "LICENSE.txt", sdk / "NOTICE.txt")
    for path in notices:
        inputs[str(path)] = sha(path)

    assets = engine / "res/xui/document_web" if include_assets else None
    if assets is not None:
        if not (assets / "renderer.html").is_file():
            raise ValueError("The selected browser package needs the local Document renderer")
        for path in sorted(assets.rglob("*")):
            if path.is_file():
                inputs[str(path)] = sha(path)
    return notices, assets


def stage_browser(out, browser):
    if browser is None:
        return
    notices, assets = browser
    target = out / "_xge_native/notices/webview2"
    target.mkdir(parents=True)
    for path in notices:
        shutil.copy2(path, target / path.name)
    if assets is not None:
        shutil.copytree(assets, out / "xui/resources/document_web")
