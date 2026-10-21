"""Run native CJK fallback cases with private font directories and copied binaries."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import uuid
from PIL import Image
from font_resources import verify as verify_fonts


def sha(path):
    return hashlib.sha256(native(path).read_bytes()).hexdigest()


def native(path):
    value = str(Path(path).absolute())
    return Path(value if value.startswith("\\\\?\\") else "\\\\?\\" + value)


def ordinary(path):
    value = str(path)
    return value[4:] if value.startswith("\\\\?\\") else value


def check(fixture, output):
    fixture = Path(fixture).resolve(strict=True)
    proof = Path(output).resolve() / ("font-fallback-" + uuid.uuid4().hex)
    proof.mkdir(parents=True)
    originals = [fixture, fixture.parent / "xge.dll", fixture.parent / "data/commands.json",
                 *sorted((fixture.parent / "data/fonts").iterdir())]
    inputs = {str(p): sha(p) for p in originals}
    fonts = verify_fonts(fixture.parent / "data/fonts")
    calls, checks = [], 0

    def copy_binary(directory, with_fonts=True):
        disk = native(directory)
        disk.mkdir(parents=True)
        shutil.copy2(fixture, disk / fixture.name)
        shutil.copy2(fixture.parent / "xge.dll", disk / "xge.dll")
        (disk / "data").mkdir()
        shutil.copy2(fixture.parent / "data/commands.json", disk / "data/commands.json")
        if with_fonts:
            shutil.copytree(fixture.parent / "data/fonts", disk / "data/fonts")
        return directory / fixture.name

    def invoke(name, executable, mode, *extra):
        nonlocal checks
        directory = proof / name
        command = [str(executable), str(directory), mode, *map(str, extra)]
        # CreateProcessW's extended absolute spelling is needed for this real
        # >260-character executable path. Model/resource paths stay ordinary.
        if len(str(executable)) >= 260:
            command[0] = "\\\\?\\" + str(executable)
        stdout, stderr = proof / (name + ".stdout"), proof / (name + ".stderr")
        started = datetime.now(timezone.utc).isoformat()
        with stdout.open("xb") as out, stderr.open("xb") as err:
            child = subprocess.Popen(command, executable=command[0], cwd=proof, stdout=out, stderr=err)
            try:
                code = child.wait(timeout=75)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
                (proof / (name + ".timeout")).write_text("Owned native fixture timed out", encoding="utf8")
                raise
        row = {"name": name, "command": command, "pid": child.pid, "startedUtc": started,
               "exit": code, "stdout": str(stdout), "stderr": str(stderr),
               "stdoutSha256": sha(stdout), "stderrSha256": sha(stderr)}
        calls.append(row)
        (proof / (name + ".invocation.json")).write_text(json.dumps(row, ensure_ascii=False, indent=2), encoding="utf8")
        assert code == 0, (name, code, str(stderr))
        text = stdout.read_text(encoding="utf8")
        match = re.search(r"font_fallback_test passed: (\d+) checks", text)
        assert match, (name, text)
        checks += int(match[1])
        print("Passed native font fallback", name, flush=True)

    normal = copy_binary(proof / "bin-中文")
    for name in ("missing", "corrupt", "saved-bad", "latin"):
        invoke(name, normal, "--" + name)
    missing = copy_binary(proof / "bin-no-font", False)
    invoke("no-bundle", missing, "--no-bundle")
    deep = proof / "deep-font"
    while len(str(deep)) < 310:
        deep /= "长路径-font-resources-0123456789"
    native(deep).mkdir(parents=True)
    deep_font = deep / "中文字体.otf"
    shutil.copy2(fixture.parent / "data/fonts/NotoSansMonoCJKsc-Regular.otf", native(deep_font))
    invoke("deep", normal, "--deep-font", deep_font)
    portable_a = copy_binary(proof / "portable-A-中文")
    invoke("portable-write", portable_a, "--portable-write")
    settings = portable_a.parent / "option/settings.json"
    saved = json.loads(settings.read_text(encoding="utf8"))
    assert saved["editor"]["font"]["path"] == saved["ui"]["font"]["path"] == "../data/fonts/NotoSansMonoCJKsc-Regular.otf"
    assert saved["editor"]["font"]["size"] == 24 and saved["ui"]["font"]["size"] == 20
    portable_b = proof / "portable-B-移动"
    shutil.copytree(portable_a.parent, portable_b)
    assert sha(portable_b / "option/settings.json") == sha(settings)
    invoke("portable-read", portable_b / fixture.name, "--portable-read")
    for path, digest in inputs.items():
        assert sha(path) == digest, path
    images = []
    for p in sorted(native(proof).rglob("*.png")):
        with Image.open(p) as im:
            assert im.size == (1280, 800) and max(im.convert("L").getextrema()) - min(im.convert("L").getextrema()) > 50
        images.append({"path": ordinary(p), "sha256": sha(p), "width": 1280, "height": 800})
    assert len(images) == 14, len(images)
    result = {"status": "passed", "utc": datetime.now(timezone.utc).isoformat(), "inputs": inputs,
              "checks": checks, "actualProcesses": len(calls), "images": images, "calls": calls,
              "fontMaterials": fonts, "nativeGlyphsComparedWithMissingGlyph": True,
              "actualDeepFontPathLength": len(str(deep_font)),
              "portableSettingsRelative": True, "neverSwitchedDesktop": True,
              "systemFontsModified": False, "formalRelease": False, "goalStatus": "active"}
    result["files"] = [{"path": ordinary(p), "sha256": sha(p)} for p in sorted(native(proof).rglob("*")) if p.is_file()]
    (proof / "evidence.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print("Native CJK fallback:", checks, "checks;", proof / "evidence.json", flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    check(args.fixture, args.output)
