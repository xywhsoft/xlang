"""Real standalone/relocation acceptance, with artifact and extraction boundaries."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import uuid
import zipfile
from package_portable import PackageError, assemble, extract_verified, inspect_zip, load_json, require, sha, tree, verify_payload, verify_development_materials, write_json


def run(arguments, log, env=None, cwd=None, timeout=240):
    with Path(log).open("wb") as output:
        result = subprocess.run([str(v) for v in arguments], stdout=output, stderr=subprocess.STDOUT, env=env, cwd=cwd, timeout=timeout)
    require(result.returncode == 0, f"Acceptance command failed ({result.returncode}); inspect {log}")
    return {"arguments": [str(v) for v in arguments], "exit": result.returncode, "log": str(log), "sha256": sha(log)}


def expect_reject(name, action, expected):
    try:
        action()
    except (PackageError, OSError, zipfile.BadZipFile) as error:
        require(expected.casefold() in str(error).casefold(), f"Unexpected rejection for {name}: {error}")
        return {"name": name, "reason": str(error)}
    raise PackageError(f"Expected rejection did not occur: {name}")


def malformed_archives(proof):
    results = []
    cases = [
        ("traversal", [("xlang-ide/../outside.txt", b"x", None)], "Invalid path"),
        ("drive", [("xlang-ide/C:/outside.txt", b"x", None)], "Invalid Windows path"),
        ("backslash", [("xlang-ide/..\\outside.txt", b"x", None)], "Invalid path"),
        ("case-duplicate", [("xlang-ide/A.txt", b"x", None), ("xlang-ide/a.txt", b"y", None)], "Duplicate ZIP"),
        ("symlink", [("xlang-ide/link", b"../outside", stat.S_IFLNK|0o777)], "links/devices"),
        ("device-name", [("xlang-ide/NUL.txt", b"x", None)], "Reserved Windows path"),
        ("file-directory-conflict", [("xlang-ide/item", b"x", None), ("xlang-ide/item/inside", b"y", None)], "path conflict"),
        ("wrong-root", [("foreign/file.txt", b"x", None)], "Invalid ZIP root"),
    ]
    for name, entries, expected in cases:
        archive = proof/("reject-"+name+".zip")
        with zipfile.ZipFile(archive, "x") as bundle:
            for path, value, mode in entries:
                item = zipfile.ZipInfo(path); item.create_system = 3
                item.external_attr = (mode if mode else stat.S_IFREG|0o644) << 16
                bundle.writestr(item, value)
        destination = proof/("rejected-extract-"+name)
        results.append(expect_reject(name, lambda: extract_verified(archive, destination), expected))
        require(not destination.exists(), "Invalid ZIP wrote an extraction directory")
    # An oversized declared member is rejected from the central directory, without
    # materializing hundreds of MiB. This tests the actual archive parser boundary.
    archive = proof/"reject-oversize.zip"
    with zipfile.ZipFile(archive, "x") as bundle:
        item = zipfile.ZipInfo("xlang-ide/oversize.bin"); item.create_system = 3
        item.external_attr = (stat.S_IFREG|0o644) << 16
        bundle.writestr(item, b"x")
    value = bytearray(archive.read_bytes())
    at = value.index(b"PK\x01\x02")
    value[at+24:at+28] = (257*1024*1024).to_bytes(4,"little")
    archive.write_bytes(value)
    destination = proof/"rejected-extract-oversize"
    results.append(expect_reject("oversize", lambda: extract_verified(archive, destination), "Oversize ZIP file"))
    require(not destination.exists(), "Oversize ZIP wrote an extraction directory")
    return results


def move_private(source, target, proof):
    source, target, proof = source.resolve(), target.absolute(), proof.resolve()
    require(source.is_relative_to(proof) and target.resolve().is_relative_to(proof) and source != proof,
            "Relocation must remain inside the owned acceptance root")
    require(not target.exists(), "Relocation target exists")
    target.parent.mkdir(parents=True, exist_ok=True)
    source.rename(target)
    require(not source.exists() and target.is_dir(), "Private package relocation failed")
    return target


def check(args):
    ide = Path(__file__).resolve().parents[1]
    candidate = Path(args.package).resolve()
    require(candidate.is_relative_to(ide/"dist"), "Acceptance requires an ide/dist candidate")
    receipt = load_json(candidate/"package-receipt.json")
    archive = candidate/"xlang-ide-windows-x64.zip"
    require(sha(archive) == receipt["archiveSha256"] and sha(candidate/"xlang-ide/package-manifest.json") == receipt["manifestSha256"], "Candidate receipt mismatch")
    manifest = verify_payload(candidate/"xlang-ide")
    materials = verify_development_materials(candidate/"xlang-ide", manifest)
    inspect_zip(archive)
    fixture = Path(args.fixture).resolve()
    private = fixture.is_relative_to(ide/"build") or (ide.parent.name == "build" and fixture.is_relative_to(ide/"release"))
    require(private and fixture.name == "portable_view_test.exe", "Native acceptance fixture must be the private built helper")
    fixture_hash = sha(fixture)
    proof = ide/"build/tests/portable"/("acceptance-"+uuid.uuid4().hex)
    proof.mkdir(parents=True)
    proof_path = ide/"build/portable-last-run.json"
    record = {"utc": datetime.now(timezone.utc).isoformat(), "status": "running", "candidate": str(candidate), "proof": str(proof), "fixtureSha256": fixture_hash, "archiveSha256": receipt["archiveSha256"], "native": [], "negative": []}
    write_json(proof_path,record)
    try:
        record["negative"] += malformed_archives(proof)
        record["negative"].append(expect_reject("existing-extraction", lambda: extract_verified(archive, proof), "already exists"))
        # Reject existing destinations before reading/copying inputs, including a
        # target whose seed settings have already been changed by a user.
        compiler_proof = load_json(materials/"provenance/compiler-inputs.json")
        compiler_root = Path(compiler_proof["compiler"]).parent
        with_c_lsp = manifest.get("features", {}).get("cLsp", False)
        options = argparse.Namespace(compiler_root=str(compiler_root), compiler_manifest=str(materials/"provenance/compiler-inputs.json"), output=str(candidate), with_c_lsp=with_c_lsp)
        if with_c_lsp:
            clangd_proof = load_json(materials/"provenance/clangd-inputs.json")
            options.c_server_root = str(Path(clangd_proof["directory"])/next(iter(clangd_proof["files"])).split("/")[0])
            options.c_server_manifest = str(materials/"provenance/clangd-inputs.json")
        else:
            require("clangd" not in manifest["toolchains"] and not (candidate/"xlang-ide/toolchains/clangd").exists(), "C LSP assets remain in the default package")
        record["negative"].append(expect_reject("existing-candidate", lambda: assemble(options), "overwrite existing"))
        outside_options = argparse.Namespace(**vars(options)); outside_options.output=str(proof/"outside-dist")
        record["negative"].append(expect_reject("output-outside-dist", lambda: assemble(outside_options), "within ide/dist"))
        user_target = ide/"dist"/(".protected-user-state-"+uuid.uuid4().hex)
        (user_target/"option").mkdir(parents=True)
        user_settings = user_target/"option/settings.json"
        write_json(user_settings,{"userData":"已修改的用户设置 😀", "fontSize":23})
        original_user_hash = sha(user_settings)
        user_options = argparse.Namespace(**vars(options)); user_options.output=str(user_target)
        record["negative"].append(expect_reject("existing-user-data", lambda: assemble(user_options), "overwrite existing"))
        require(sha(user_settings)==original_user_hash and len(tree(user_target))==1, "Existing user data was changed")
        record["protectedUserData"]={"path":str(user_settings),"sha256":original_user_hash}
        payload = extract_verified(archive, proof/"初次 中文 空格解压")
        # Real copied assets, not an invented reduced manifest.
        catalog = payload/"data/commands.json"; original_catalog = catalog.read_bytes()
        catalog.write_bytes(original_catalog+b"\n")
        try:
            record["negative"].append(expect_reject("tampered-catalog", lambda: verify_payload(payload), "Payload mismatch: data/commands.json"))
        finally:
            catalog.write_bytes(original_catalog)
        missing = payload/"include/xlang.h"
        if not missing.exists():
            missing = next((payload/"include").rglob("*.h"))
        temporary = proof/"owned-header-original"
        original_header_hash = sha(missing); missing.rename(temporary)
        try:
            record["negative"].append(expect_reject("missing-runtime-header", lambda: verify_payload(payload), str(missing.name)))
        finally:
            temporary.rename(missing)
        require(sha(missing) == original_header_hash, "Private header restoration failed")
        verify_payload(payload)
        seed = load_json(payload/"option/settings.json")
        seed["packageFixture"] = {"sentinel": "中文 😀 retained", "unknown": [42, "keep"]}
        write_json(payload/"option/settings.json", seed)
        env = dict(os.environ)
        windows_root = next(value for key, value in env.items() if key.upper()=="SYSTEMROOT")
        env["PATH"] = os.pathsep.join((windows_root, str(Path(windows_root)/"System32"), str(Path(windows_root)/"System32/Wbem")))
        for key in list(env):
            if key.upper() in ("XLANG_COMPILER", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH"):
                del env[key]
        record["runtimeEnvironment"] = {"PATH": env["PATH"], "compilerOverridePresent": any(k.upper()=="XLANG_COMPILER" for k in env)}
        evidence = proof/"独立运行 证据"; evidence.mkdir()
        help_command = next(p["id"] for p in load_json(catalog, limit=64 * 1024 * 1024)["entries"] if p.get("sections"))
        for phase in ("initial", "moved"):
            verify_payload(payload, allow_user_state=True)
            # Test EXE is deliberately injected only into the private extraction,
            # removed after the run, and never shipped in the candidate/ZIP.
            helper = payload/fixture.name
            require(not helper.exists(), "Native helper already exists in package copy")
            shutil.copyfile(fixture,helper)
            require(sha(helper) == fixture_hash, "Copied native fixture mismatch")
            try:
                record["native"].append(run([helper,evidence,phase], proof/(phase+"-native.log"), env, proof))
            finally:
                require(helper.resolve().is_relative_to(proof), "Helper path escaped acceptance root")
                helper.unlink()
            settings = load_json(payload/"option/settings.json")
            require(settings["toolchain"] == seed["toolchain"] and settings["packageFixture"] == seed["packageFixture"], "Tool defaults/unknown preferences were not retained")
            require(settings["ui"]["font"]["size"] == 18 and settings["editor"]["font"]["size"] == 18, "User font sizes were not retained")
            frontends = [name for name in ("xlang_ide.exe", "xlang_ide_console.exe") if name in manifest["corePE"]]
            for binary in frontends:
                screenshot = proof/(phase+"-"+binary+".png")
                command = [payload/binary,"--frames","3","--hidden","--no-session","--settings",payload/"option/settings.json","--command",help_command,"--screenshot",screenshot]
                record["native"].append(run(command, proof/(phase+"-"+binary+".log"), env, proof, timeout=60))
                require(screenshot.is_file() and screenshot.stat().st_size > 20000, "Packaged executable did not capture a native render")
            verify_payload(payload, allow_user_state=True)
            if phase == "initial":
                first_path = payload
                payload = move_private(payload, proof/"迁移后 中文 空格目录"/"xlang-ide", proof)
                require(not first_path.exists(), "Initial runtime root still exists after relocation")
                record["initialRootRemoved"] = str(first_path)
        results = list(evidence.rglob("native-result.json"))
        require(len(results)==2, "Missing standalone native evidence")
        for path in results:
            value=load_json(path)
            require(value.get("explicitTest") is True, "Native fixture did not run the explicitly configured C/xlang test program")
            expected = payload if value["moved"] else Path(record["initialRootRemoved"])
            require(Path(value["program"]).resolve() == expected.resolve(), "Fixture used an external program directory")
            require(Path(value["compiler"]).resolve() == (expected/"xl.exe").resolve(), "Fixture used an external compiler")
            if with_c_lsp:
                require(Path(value["cServer"]).resolve() == (expected/"toolchains/clangd/bin/clangd.exe").resolve(), "Fixture used an external C server")
            else:
                require("cServer" not in value, "Default fixture unexpectedly used a C server")
        record["renders"]=[{"path":str(p),"sha256":sha(p),"size":p.stat().st_size} for p in proof.rglob("*.png")]
        require(len(record["renders"])==8+2*len(frontends), "Expected native fixture and packaged frontend renders")
        # Assembly and use of disposable copies must leave the delivered candidate
        # and the original frozen providers unchanged.
        verify_payload(candidate/"xlang-ide")
        verify_development_materials(candidate/"xlang-ide", manifest)
        require(sha(archive) == receipt["archiveSha256"], "Candidate archive changed")
        changed = [p["path"] for p in receipt["inputs"] if sha(p["path"]) != p["sha256"]]
        require(not changed, f"Original package inputs changed: {changed[:3]}")
        require(sha(fixture)==fixture_hash, "Original native helper changed")
        record["status"]="passed"; record["payloadFiles"]=len(manifest["files"])+1
        record["negativeGates"]=len(record["negative"])
        write_json(proof/"portable-verification.json",record); write_json(proof_path,record)
        print(f"Portable acceptance passed: {record['payloadFiles']} payload files, {record['negativeGates']} actual artifact/extraction rejection gates, 2 default native runs with relocation, {2*len(frontends)} packaged executable runs, {len(record['renders'])} fresh PNGs; evidence {proof}")
    except Exception as error:
        record["status"]="failed"; record["error"]=str(error)
        write_json(proof/"portable-verification.json",record); write_json(proof_path,record)
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package",required=True)
    parser.add_argument("--fixture",required=True)
    args=parser.parse_args()
    try:
        check(args)
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired, zipfile.BadZipFile) as error:
        parser.exit(1,f"Portable acceptance failed: {error}\n")

if __name__=="__main__":
    main()
