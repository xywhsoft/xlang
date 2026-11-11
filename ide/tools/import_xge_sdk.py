"""Stage a matched XGE 3 IDE SDK from an isolated, completed upstream build.

This does not install the SDK or alter the upstream repository. Source materials
come from compiler dependency records, the git archive and explicit pinned patches.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf8')


def runtime_declarations(source):
    """Use the declarations from the exact single header compiled into XGE.

    An upstream xrt_decl.h can lag behind local native ownership additions.
    The single header closes all declaration guards before its implementation;
    do not mix that runtime with a declaration snapshot from another revision.
    """
    raw = (source / 'extlibs/xrt/xrt.h').read_bytes()
    marker = b'#if defined(XRT_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION_ONCE)'
    if raw.count(marker) != 1:
        raise ValueError('Unrecognized XRT single-header implementation boundary')
    prefix = raw.split(marker, 1)[0]
    depth = 0
    for line in prefix.decode('utf8').splitlines():
        if re.match(r'^\s*#\s*(if|ifdef|ifndef)\b', line):
            depth += 1
        elif re.match(r'^\s*#\s*endif\b', line):
            depth -= 1
        if depth < 0:
            raise ValueError('Invalid XRT declaration guards')
    if depth:
        raise ValueError('XRT declaration boundary has unclosed guards')
    # Public IDE headers also include <xrt.h> directly. Select the same runtime
    # profile before the single-header guards can freeze feature selection.
    return b'#include "xrt_config.h"\n' + prefix


def stage(args):
    source, build, output = args.source.resolve(), args.build.resolve(), args.output.absolute()
    if output.exists() or output.is_relative_to(source):
        raise ValueError('Use a new staging directory outside the source tree')
    report = read(build / 'build-report.json')
    if report['profile'] != 'ide' or report['api_major'] != 3:
        raise ValueError('A completed upstream XGE 3 IDE-profile build is required')
    header = (source / 'include/xge.h').read_text(encoding='utf8')
    version = '.'.join(re.search(r'#define\s+XGE_VERSION_' + n + r'\s+(\d+)', header)[1]
                       for n in ('MAJOR', 'MINOR', 'PATCH'))
    proxy_version = int(re.search(r'#define\s+XUI_PROXY_VERSION\s+(\d+)',
                                 (source / 'include/xui.h').read_text(encoding='utf8'))[1])
    patch_info = read(args.patch_manifest) if args.patch_manifest else None
    patches = {r['path']: r for r in patch_info['files']} if patch_info else {}
    if patch_info and (patch_info['format'] != 'xlang-ide-xge-source-patches-1' or
                       patch_info['baseCommit'] != args.commit or len(patches) != len(patch_info['files'])):
        raise ValueError('Invalid compatibility patch manifest')
    for name in ('xge.dll', 'xge.lib', 'xge_build_config.h'):
        if not (build / name).is_file():
            raise ValueError('Missing SDK output: ' + name)
    entrypoints = ['build.bat', 'tools/build/build.py', 'tools/build/build_dll.bat',
                   'tools/build/host_xrt.py', 'tools/build/relink.py',
                   'config/profiles.json', 'config/modules.json', 'config/includes.rsp',
                   'platform/windows/xge.rc', 'res/xge/icons/xx_dll.ico']
    entrypoints += [f'extlibs/xrt/extlibs/{p}/config/modules.json'
                    for p in ('xllm', 'xllm-session', 'xwork')]
    notices = [
        ('libunibreak', 'extlibs/libunibreak/LICENCE', 'named-notice'),
        ('libunibreak', 'extlibs/libunibreak/UNICODE-LICENSE.txt', 'named-notice'),
        ('md4c', 'extlibs/md4c/LICENSE.md', 'named-notice'),
        ('zstd', 'extlibs/zstd/LICENSE', 'named-notice'),
        ('emoji', 'res/xge/emoji/twemoji_core/17.0/LICENSE-GRAPHICS', 'named-notice'),
        ('emoji', 'res/xge/emoji/twemoji_core/17.0/NOTICE.md', 'named-notice'),
        ('dockpanel', 'res/xui/dockpanel_suite/LICENSE.dockpanelsuite.txt', 'named-notice'),
        ('harfbuzz', 'extlibs/harfbuzz/COPYING', 'named-notice'),
        ('sheenbidi', 'extlibs/sheenbidi/LICENSE', 'named-notice'),
        ('stb', 'extlibs/stb/stb_image.h', 'embedded-notice-header'),
        ('stb', 'extlibs/stb/stb_image_write.h', 'embedded-notice-header'),
        ('stb', 'extlibs/stb/stb_truetype.h', 'embedded-notice-header'),
        ('sokol', 'extlibs/sokol/sokol_app.h', 'embedded-notice-header'),
        ('miniaudio', 'extlibs/miniaudio/miniaudio.h', 'embedded-notice-header'),
        ('xrt', 'extlibs/xrt/xrt.h', 'embedded-notice-header')]
    dependencies = set()
    cache_files = [*list((build / 'obj').glob('*.json')),
                   *list((source / 'build/shared/obj').glob('*.json'))]
    for cache in cache_files:
        if cache.name in ('xge_res.json', 'xgedbg_res.json'):
            continue  # windres uses a plain fingerprint, not compiler JSON.
        data = read(cache)
        if not isinstance(data, dict) or 'deps' not in data:
            continue
        for name in data['deps']:
            path = Path(name)
            path = (source / path).resolve() if not path.is_absolute() else path.resolve()
            if path.is_relative_to(source) and not path.is_relative_to(source / 'build'):
                dependencies.add(path.relative_to(source).as_posix())
    units = [*report['sources'], 'extlibs/harfbuzz/src/harfbuzz.cc']
    if not set(units).issubset(dependencies):
        raise ValueError('Compiler dependency records omit SDK translation units')
    public = [p.relative_to(source).as_posix() for p in (source / 'include').glob('*.h')]
    selected = dependencies | set(entrypoints) | set(public) | {n for _, n, _ in notices}
    selected |= {'extlibs/xrt/xrt_decl.h', 'extlibs/xrt/upstream.json'}
    rows = []
    with zipfile.ZipFile(args.archive) as archive:
        if archive.comment.decode('ascii') != args.commit or not re.fullmatch(r'[0-9a-f]{40}', args.commit):
            raise ValueError('Commit does not match the original git archive')
        if not set(patches).issubset(selected):
            raise ValueError('Compatibility patch is outside the actual source closure')
        for name in sorted(selected):
            path = source / name
            if name in archive.namelist():
                original = archive.read(name)
                if name in patches and patches[name].get('upstreamExists') is False:
                    raise ValueError('Added source already exists in archive: ' + name)
            else:
                if name not in patches or patches[name].get('upstreamExists') is not False:
                    raise ValueError('Unrecorded source addition: ' + name)
                original = b''
            digest = hashlib.sha256(original).hexdigest()
            actual = sha(path)
            if name in patches:
                patch = patches[name]
                payload = (args.patch_manifest.parent / patch['payload']).resolve()
                if not payload.is_relative_to(args.patch_manifest.parent.resolve()) or not payload.is_file():
                    raise ValueError('Unsafe compatibility patch payload')
                if patch['upstreamSha256'] != digest or patch['sha256'] != actual or sha(payload) != actual:
                    raise ValueError('Compatibility patch hash mismatch: ' + name)
            elif actual != digest:
                raise ValueError('Source changed since the frozen upstream commit: ' + name)
            rows.append(dict(path=name, sha256=actual, size=path.stat().st_size))
    output.mkdir(parents=True)
    sdk = output / 'sdk'; sdk.mkdir()
    def copy(src, name):
        target = sdk / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, target)
    for name in public:
        copy(source / name, Path(name).name)
    for name in ('xge.dll', 'xge.lib', 'xge_build_config.h'):
        copy(build / name, name)
    declarations = runtime_declarations(source)
    (sdk / 'extlibs/xrt').mkdir(parents=True, exist_ok=True)
    (sdk / 'extlibs/xrt/xrt.h').write_bytes(declarations)
    write(sdk / 'provenance/xrt-declarations.json', dict(
        schemaVersion=1, source='extlibs/xrt/xrt.h', sourceSha256=sha(source / 'extlibs/xrt/xrt.h'),
        operation='matched xrt_config.h include followed by exact byte prefix before the single implementation boundary',
        output='extlibs/xrt/xrt.h', outputSha256=hashlib.sha256(declarations).hexdigest(),
        outputBytes=len(declarations), upstreamDeclarationSha256=sha(source / 'extlibs/xrt/xrt_decl.h')))
    for original, target in [('xrt_config.h', 'xrt_config.h'), ('upstream.json', 'upstream.json')]:
        copy(source / 'extlibs/xrt' / original, 'extlibs/xrt/' + target)
    for name in sorted(dependencies):
        if name.startswith('extlibs/xrt/') and name.endswith('.h') and name not in (
                'extlibs/xrt/xrt.h', 'extlibs/xrt/xrt_decl.h', 'extlibs/xrt/xrt_config.h'):
            copy(source / name, name)
    copy(args.baseline / 'sdk_manifest.json', 'provenance/parent-sdk-manifest.json')
    patch_materials = []
    if patch_info:
        for name in ['patch-manifest.json', *[r[k] for r in patch_info['files'] for k in ('payload', 'diff')]]:
            origin = args.patch_manifest if name == 'patch-manifest.json' else args.patch_manifest.parent / name
            if not origin.resolve().is_relative_to(args.patch_manifest.parent.resolve()):
                raise ValueError('Unsafe compatibility patch material')
            target = 'provenance/compatibility-patches/' + name
            copy(origin, target)
            patch_materials.append(target)
    source_info = dict(schemaVersion=1, root=str(source), sourceCommit=args.commit,
                       sourceFileCount=len(rows), files=rows, fullSourceTreeNotIncludedInConsumerSdk=True,
                       compatibilityPatches=patch_info)
    write(sdk / 'xge-source-files.json', source_info)
    write(sdk / 'provenance/source-build-manifest.json', source_info)
    build_info = dict(status='sdk-build-finished', exitCode=0, profile='ide', version=version,
                      source=str(source), sourceCommit=args.commit, sourceUnchanged=True,
                      sourceArchiveSha256=sha(args.archive), wholeSdkWerror=False,
                      compiler=report['compiler'][2].splitlines()[0], upstreamBuild=report,
                      compatibilityPatches=patch_info,
                      initialCleanBuild=(read(build / 'build-report-upstream-clean.json')
                                         if patches and (build / 'build-report-upstream-clean.json').is_file() else None),
                      outputs=[dict(path=str(build / n), sha256=sha(build / n))
                               for n in ('xge.dll', 'xge.lib', 'xge_build_config.h')])
    write(sdk / 'provenance/source-build-report.json', build_info)
    baseline = read(args.baseline / 'sdk_manifest.json')
    # These were private SDK67 APIs; XGE 3 supplies retained public text items.
    retired = {'xuiCodeEditSetTextShapeVisitor', 'xuiCodeEditSetTextShapeCursorVisitor',
               'xuiProxyXgeVisitTextShape', 'xuiProxyXgeTextShapeCursorFree', 'xuiProxyXgeVisitTextShapeCached'}
    required_exports = [n for n in baseline['requiredExports'] if n not in retired]
    xui_header = (source / 'include/xui.h').read_text(encoding='utf8')
    for name in ('xuiCodeEditSetTokenStyleResolver', 'xuiCodeEditGetTokenStyleResolver',
                 'xuiCodeDocumentBatchPrepare', 'xuiCodeDocumentBatchCommit',
                 'xuiCodeDocumentBatchDestroy',
                 'xuiTerminalSetNewlineMode', 'xuiTerminalGetNewlineMode',
                 'xuiDockPanelSetTabScrollButtonsVisible',
                 'xuiDockPanelGetTabScrollButtonsVisible', 'xuiDockPanelGetPaneTabScrollRects',
                 'xuiCodeFindScopeSetActiveEditor', 'xuiCodeFindScopeGetActiveEditor',
                 'xuiCodeFindScopeGetFindWindow', 'xuiCodeFindScopeFindNext',
                 'xuiCodeFindScopeRefresh'):
        if re.search(r'\b' + name + r'\s*\(', xui_header) and name not in required_exports:
            required_exports.append(name)
    # Historical dependency reports keep their original paths and bytes. Give
    # this import a separate tree so relative patch payload references still work.
    evidence_root = 'provenance/source-' + args.commit[:12] + '-' + sha(build / 'build-report.json')[:12]
    generated = [(p.relative_to(sdk / 'provenance').as_posix(), p.read_bytes())
                 for p in sorted((sdk / 'provenance').rglob('*')) if p.is_file()]
    for name, raw in generated:
        target = sdk / evidence_root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
    historical = args.baseline / 'provenance'
    if historical.is_dir():
        for origin in sorted(historical.rglob('*')):
            if origin.is_file():
                target = sdk / 'provenance' / origin.relative_to(historical)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(origin, target)
    patch_materials = [evidence_root + '/' + name.removeprefix('provenance/')
                       for name in patch_materials]
    source_info['compatibilityPatchRoot'] = evidence_root + '/compatibility-patches'
    write(sdk / 'xge-source-files.json', source_info)
    manifest = dict(schemaVersion=2, profile='xlang-ide-xge3-matched-set', version=version,
                    policy=baseline['policy'], buildDefines=['XGE_XRT_PROFILE_IDE'],
                    xrtDirectory='extlibs/xrt', configurationHeader='xge_build_config.h',
                    requiredXrtModules=baseline['requiredXrtModules'],
                    requiredExports=required_exports,
                    privateCandidate=True, formalRelease=False,
                    provenance=dict(builtAtUtc=datetime.now(timezone.utc).isoformat(),
                        materialsRoot=evidence_root,
                        sourceBuildReport=evidence_root + '/source-build-report.json',
                        sourceBuildManifest=evidence_root + '/source-build-manifest.json',
                        sourceRepository='https://gitee.com/xywhsoft/xge', sourceCommit=args.commit,
                        sourceWorktreeDirty=bool(patches), upstreamArchiveClean=True,
                        version=version, proxyVersion=proxy_version, compatibilityPatchFiles=patch_materials,
                        compiler=build_info['compiler'], xrt=read(source / 'extlibs/xrt/upstream.json'),
                        sourceBuildReportSha256=sha(sdk / evidence_root / 'source-build-report.json'),
                        sourceBuildManifestSha256=sha(sdk / evidence_root / 'source-build-manifest.json')))
    manifest['files'] = [dict(path=p.relative_to(sdk).as_posix(), sha256=sha(p))
                         for p in sorted(sdk.rglob('*')) if p.is_file()]
    write(sdk / 'sdk_manifest.json', manifest)
    materials = dict(format='xlang-ide-sdk-materials-1', sourceManifestSha256=sha(sdk / 'xge-source-files.json'),
                     sdkDllSha256=sha(sdk / 'xge.dll'), sourceScope='XGE ' + version + ' IDE-profile compiler dependency closure, build inputs, public headers and notices; explicit compatibility patches recorded in provenance',
                     translationUnits=units, preprocessorDependencies=sorted(dependencies), buildEntrypoints=entrypoints,
                     materials=[dict(component=c, source=n, kind=k) for c, n, k in notices], files=rows,
                     legalDistributionReviewComplete=False, formalRelease=False, sourceCommit=args.commit)
    write(output / 'xge-sdk-materials.json', materials)
    write(output / 'import-result.json', dict(status='sdk-staged', sdkRoot=str(sdk), sourceFiles=len(rows),
                                             translationUnits=len(units), sourceCommit=args.commit,
                                             dllSha256=sha(sdk / 'xge.dll')))
    print(json.dumps(dict(status='sdk-staged', sdkRoot=str(sdk), sourceFiles=len(rows), translationUnits=len(units))))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('source', 'build', 'archive', 'baseline', 'output'):
        parser.add_argument('--' + option, type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--patch-manifest', type=Path)
    stage(parser.parse_args())
