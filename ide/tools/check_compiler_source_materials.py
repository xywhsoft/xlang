"""Check real frozen compiler materials and private corruptions without rebuilding providers."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
from types import SimpleNamespace

from package_portable import (PackageError, MAX_FILES, assemble, expected_inputs,
                             load_json, plain, require, safe_name, sha, write_json)
from compiler_release_materials import collect


def check(args):
    ide = Path(__file__).resolve().parents[1]
    manifest, report = Path(args.source_manifest), Path(args.build_report)
    spec = load_json(manifest)
    base = Path(spec['directory'])
    _, _, _, runtime = expected_inputs(args.compiler_manifest, base / 'release')
    proof = ide / 'build/tests/compiler-source-materials' / uuid.uuid4().hex
    proof.mkdir(parents=True)
    cases = []

    def run(source=manifest, build=report, provider=base, maximum=MAX_FILES):
        registered = []
        def add(path, target, digest=None):
            plain(path)
            require(digest and sha(path) == digest, 'Registered compiler material source differs')
            registered.append({'source': str(path), 'target': target, 'sha256': digest})
        value = collect(source, build, provider, runtime, add=add, load_json=load_json,
                        plain=plain, safe_name=safe_name, sha=sha, require=require, max_files=maximum)
        return value, registered

    def accept(name, result):
        require(result, name)
        cases.append({'case': name, 'status': 'passed'})

    def reject(name, action, expected):
        try:
            action()
        except (PackageError, OSError) as error:
            require(expected.lower() in str(error).lower(), f'Wrong rejection for {name}: {error}')
            cases.append({'case': name, 'status': 'rejected', 'reason': str(error)})
        else:
            raise PackageError(f'Accepted invalid compiler source materials: {name}')

    info, registered = run()
    accept('all-frozen-sources-and-build-entries', info['sourceFiles'] == 1521 and
           info['rootBuildScripts'] == 11 and info['copiedFiles'] == 1532)
    accept('dll-fixture-dependency-closure', len(info['supplementalBuildInputs']) == 9 and
           'test/cases/xl_dll_module_dependency_lifetime.c' in info['supplementalBuildInputs'])
    accept('actual-independent-build-evidence', info['independentBuild']['status'] ==
           'recorded-independent-source-build-passed')
    accept('runtime-artifact-binding', info['runtimeArtifacts'] == {n: runtime[n] for n in ('xl.exe','xl.dll','xlc.dll')})
    accept('exact-tcc-source-and-copying', any(r['target'] == 'sources/xlang/lib/tcc/COPYING' for r in registered))
    accept('legal-review-remains-pending', info['legalDistributionReviewComplete'] is False and info['relinkReviewComplete'] is False)
    missing, empty = run(None, None)
    accept('missing-source-is-explicit', missing['copiedFiles'] == 0 and not empty)
    pending, _ = run(build=None)
    accept('missing-build-evidence-is-explicit', pending['independentBuild']['status'] == 'independent-build-evidence-not-supplied')
    reject('build-report-without-source', lambda: run(None, report), 'needs a source manifest')
    reject('inventory-budget', lambda: run(maximum=1532), 'inventory exceeds budget')

    def variant(name, mutate, value=spec):
        edited = json.loads(json.dumps(value))
        mutate(edited)
        path = proof / (name + '.json')
        write_json(path, edited)
        return path

    for name, mutate, message in (
        ('unsupported-format', lambda v: v.update(format='unknown'), 'Unsupported'),
        ('different-provider', lambda v: v.update(directory=str(proof)), 'runtime provider'),
        ('unsafe-path', lambda v: v['files'][0].update(path='../outside.c'), 'Invalid path'),
        ('reserved-name', lambda v: v['files'][0].update(path='src/NUL.c'), 'Reserved'),
        ('case-duplicate', lambda v: v['files'].append(dict(v['files'][0], path=v['files'][0]['path'].upper())), 'Duplicate'),
        ('wrong-source-hash', lambda v: v['files'][0].update(sha256='0'*64), 'source differs'),
        ('invalid-source-hash', lambda v: v['files'][0].update(sha256=''), 'Invalid compiler source digest'),
        ('source-omitted', lambda v: v['files'].pop(0), 'Original compiler source inventory'),
        ('dll-fixture-omitted', lambda v: v['files'].pop(), 'build dependency sources'),
        ('supplement-list-omitted', lambda v: v.update(supplementalBuildInputs=[]), 'build dependency sources'),
        ('nested-root-script', lambda v: v['rootBuildScripts'][0].update(path='tools/build.bat'), 'root build script'),
        ('build-entry-omitted', lambda v: v.update(rootBuildScripts=[r for r in v['rootBuildScripts'] if r['path']!='build.bat']), 'entry points'),
        ('original-manifest-hash', lambda v: v.update(originalSourceManifestSha256='0'*64), 'manifest differs'),
        ('changes-hash-missing', lambda v: v.update(changesSha256=''), 'changes digest'),
    ):
        path = variant(name, mutate)
        reject(name, lambda p=path: run(p, None), message)

    build_spec = load_json(report)
    for name, mutate, message in (
        ('failed-build-report', lambda v: v.update(status='failed'), 'not successful'),
        ('different-build-source', lambda v: v.update(sourceManifestSha256='0'*64), 'different source manifest'),
        ('different-runtime', lambda v: v['runtimeArtifacts'].update(**{'xl.dll':'0'*64}), 'runtime artifacts'),
        ('different-build-count', lambda v: v.update(sourceFiles=1), 'inventory differs'),
        ('rebuilt-output-omitted', lambda v: v['rebuiltArtifacts'].pop('xl.dll'), 'incomplete'),
        ('log-hash-missing', lambda v: v.update(buildLogSha256=''), 'log digest'),
        ('log-bytes-mismatch', lambda v: v.update(buildLogSha256='0'*64), 'raw build log differs'),
        ('raw-run-digest-missing', lambda v: v.update(actualRunSha256=''), 'raw evidence digests'),
        ('raw-run-bytes-mismatch', lambda v: v.update(actualRunSha256='0'*64), 'raw evidence differs'),
    ):
        path = variant(name, mutate, build_spec)
        reject(name, lambda p=path: run(build=p), message)

    if 'compilerSourceInventory' in build_spec:
        for name, mutate in (
            ('raw-inventory-digest-missing', lambda v: v.update(compilerSourceInventorySha256='')),
            ('raw-inventory-bytes-mismatch', lambda v: v.update(compilerSourceInventorySha256='0'*64)),
        ):
            path = variant(name, mutate, build_spec)
            reject(name, lambda p=path: run(build=p), 'raw inventory differs')

    private = proof / 'source'
    for row in spec['files'] + spec['rootBuildScripts']:
        target = private / row['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(base / row['path'], target)
    old = load_json(spec['originalSourceManifest'])
    old['directory'] = str(private)
    old_path = proof / 'private-original.json'
    write_json(old_path, old)
    private_spec = json.loads(json.dumps(spec))
    private_spec.update(directory=str(private), originalSourceManifest=str(old_path), originalSourceManifestSha256=sha(old_path))
    private_manifest = proof / 'private-manifest.json'
    write_json(private_manifest, private_spec)
    victim = private / spec['files'][0]['path']
    original = victim.read_bytes()
    victim.write_bytes(bytes([original[0]^1])+original[1:])
    reject('same-size-source-change', lambda: run(private_manifest, None, private), 'source differs')
    victim.unlink()
    reject('missing-private-source', lambda: run(private_manifest, None, private), victim.name)
    victim.write_bytes(original)
    directory = private / 'src'
    saved = private / 'saved-src'
    require(directory.resolve().is_relative_to(private.resolve()) and saved.resolve().is_relative_to(private.resolve()), 'Private move escaped proof')
    directory.rename(saved)
    call = subprocess.run([os.environ['COMSPEC'],'/d','/c','mklink','/J',str(directory),str(saved)], capture_output=True)
    require(call.returncode == 0, f'Private junction creation failed: {call.stdout!r} {call.stderr!r}')
    reject('reparse-source-directory', lambda: run(private_manifest, None, private), 'reparse')
    directory.rmdir()
    saved.rename(directory)
    restored, _ = run(private_manifest, None, private)
    accept('restored-private-inventory', restored['copiedFiles'] == 1532)

    bad = variant('assembly-unsafe', lambda v: v['files'][0].update(path='../outside.c'))
    target = ide / 'dist' / ('rejected-source-materials-' + uuid.uuid4().hex)
    stages = set((ide/'dist').glob('.portable-stage-*'))
    options = SimpleNamespace(compiler_root=str(base/'release'), c_server_root=args.c_server_root,
        compiler_manifest=args.compiler_manifest, c_server_manifest=args.c_server_manifest,
        compiler_source_manifest=str(bad), compiler_source_build_report=None, sdk_source_root=None, output=str(target))
    reject('assembly-rejects-before-stage', lambda: assemble(options), 'Invalid path')
    accept('assembly-created-no-stage-or-target', not target.exists() and set((ide/'dist').glob('.portable-stage-*')) == stages)
    evidence = {'status':'passed','utc':datetime.now(timezone.utc).isoformat(),'sourceManifest':str(manifest),
        'sourceManifestSha256':sha(manifest),'buildReport':str(report),'buildReportSha256':sha(report),
        'sourceMaterials':info,'checks':len(cases),'cases':cases,'inputs':registered,'formalRelease':False}
    write_json(proof/'evidence.json',evidence)
    print(json.dumps({'path':str(proof/'evidence.json'),'checks':len(cases)},ensure_ascii=False),flush=True)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source-manifest','build-report','compiler-manifest','c-server-root','c-server-manifest'):
        parser.add_argument('--'+name,required=True)
    check(parser.parse_args())
