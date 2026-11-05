"""Stage a normal-builder XGE IDE SDK without changing the installed SDK.

The IDE is a separate process and imports its XRT from the IDE-profile engine.
Language programs instead use the separately staged host-XRT engine package.
Public headers, configuration, import library and image remain a matched set.
"""
from pathlib import Path
import argparse, hashlib, json, re, shutil
from stage_xge3_packages import public_abi_inputs


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native-root','engine-build','source-manifest','parent-sdk','out','report'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,native,build,source_manifest,parent,out,report=[v.resolve() for v in (
        args.root,args.native_root,args.engine_build,args.source_manifest,args.parent_sdk,args.out,args.report)]
    assert not out.exists() and not report.exists()
    for target in (out,report):
        assert not target.is_relative_to(root/'release')
        assert not target.is_relative_to(native) and not target.is_relative_to(build)
        assert not target.is_relative_to(parent)
    assert not report.is_relative_to(out)
    frozen=read(source_manifest)
    for row in frozen['files']:
        assert sha(native/row['path'])==row['sha256'],row['path']
    metadata=read(build/'build-report.json')
    assert metadata['api_major']==3 and metadata['public_abi_schema']==2
    assert metadata['profile']=='ide' and metadata['xrt_mode']=='embedded' and metadata['linked']
    assert not metadata['runtime_dependencies'] and '-O2' in metadata['flags']
    assert [v for v in metadata['flags'] if v.startswith('-DXGE_DEBUGMODE=')]==['-DXGE_DEBUGMODE=0']
    config=(build/'xge_build_config.h').read_text(encoding='utf-8')
    match=re.search(r'^#define XGE_BUILD_ABI_FINGERPRINT UINT64_C\(0x([0-9a-f]{16})\)$',config,re.M)
    assert match and '#define XGE_XRT_PROFILE_IDE\n' in config
    actual=hashlib.sha256((config[:match.start()]+public_abi_inputs(native)+metadata['compiler'][2]+'0').encode()).hexdigest()[:16]
    assert actual==match.group(1),'SDK public ABI fingerprint differs from the actual build'
    upstream=read(native/'extlibs/xrt/upstream.json')
    declaration=root/'release/win_x64/include/xrt_decl.h'
    assert sha(native/'extlibs/xrt/xrt.h')==upstream['header_sha256']
    assert sha(declaration)==upstream['declaration_header_sha256']
    original=read(parent/'sdk_manifest.json')
    assert original['schemaVersion']==2 and original['policy']==dict(
        runtimeOwner='xge.dll',xrtLinkage='shared',replaceAsMatchedSetOnly=True)
    for row in original['files']:
        assert sha(parent/row['path'])==row['sha256'],'Parent SDK changed: '+row['path']
    inputs={str(source_manifest):sha(source_manifest),str(parent/'sdk_manifest.json'):sha(parent/'sdk_manifest.json')}
    materials={}
    def add(source,name):
        assert name not in materials and source.is_file(),name
        materials[name]=source;inputs[str(source)]=sha(source)
    for source in sorted((native/'include').glob('*.h')):
        add(source,source.name)
    for name in ('xge.dll','xge.lib','xge_build_config.h'):
        add(build/name,name)
    add(declaration,'extlibs/xrt/xrt.h')
    for name in ('xrt_config.h','upstream.json'):
        add(native/'extlibs/xrt'/name,'extlibs/xrt/'+name)
    # Preserve the dependency-owned public include structure required by the
    # IDE profile's features headers, without copying C implementation units.
    for family in ('xwork','xllm','xllm-session'):
        base=native/'extlibs/xrt/extlibs'/family
        for source in sorted(base.rglob('*')):
            if source.is_file() and source.suffix in ('.h','.inc'):
                add(source,source.relative_to(native).as_posix())
    # Dependency-owned historical reports keep their bytes and relative paths.
    # New evidence has a distinct location; source-build-report.json is not a
    # mutable alias that can silently replace an earlier acceptance report.
    for row in original['files']:
        if row['path'].startswith('provenance/'):
            add(parent/row['path'],row['path'])
    evidence='provenance/xge3-'+sha(source_manifest)[:12]+'-'+sha(build/'build-report.json')[:12]+'-'+sha(parent/'sdk_manifest.json')[:8]+'/'
    for source,name in ((source_manifest,evidence+'source-freeze.json'),
        (build/'build-report.json',evidence+'source-build-report.json'),
        (parent/'sdk_manifest.json',evidence+'parent-sdk-manifest.json'),
        (parent/'xge-source-files.json',evidence+'parent-xge-source-files.json')):
        add(source,name)
    out.mkdir(parents=True)
    for name,source in materials.items():
        target=out/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        assert sha(target)==inputs[str(source)]
    descriptor=dict(schemaVersion=1,root=str(native),files=frozen['files'],
        sourceManifestSha256=sha(source_manifest),fullSourceTreeNotIncludedInConsumerSdk=True)
    (out/'xge-source-files.json').write_text(json.dumps(descriptor,indent=2)+'\n',encoding='utf-8')
    files=[dict(path=p.relative_to(out).as_posix(),sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file()]
    manifest=dict(schemaVersion=2,profile='xlang-ide-xge3-matched-set',version='3.0.0',
        policy=original['policy'],buildDefines=['XGE_XRT_PROFILE_IDE'],xrtDirectory='extlibs/xrt',
        configurationHeader='xge_build_config.h',requiredXrtModules=original['requiredXrtModules'],
        requiredExports=original['requiredExports'],privateCandidate=True,formalRelease=False,
        provenance=dict(sourceCommit=frozen['baseHEAD'],sourceWorktreeDirty=bool(frozen['overlay']),
            sourceFreeze=evidence+'source-freeze.json',sourceFreezeSha256=sha(source_manifest),
            sourceBuildReport=evidence+'source-build-report.json',sourceBuildReportSha256=sha(build/'build-report.json'),
            proxyVersion=18,publicAbiSchema=2,publicAbiFingerprint=actual,xrt=upstream),files=files)
    (out/'sdk_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    assert all(sha(path)==digest for path,digest in inputs.items()),'A staging input changed'
    receipt=dict(complete=True,passed=True,shippingDelivery=False,testsRun=False,inputsUnchanged=True,
        inputs=inputs,outputs={p.relative_to(out).as_posix():sha(p) for p in sorted(out.rglob('*')) if p.is_file()},
        publicAbiFingerprint=actual,xrtOwner='xge.dll',runtimeDirectoryWritten=False)
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print('Staged matched IDE SDK:',out)


if __name__=='__main__':
    main()
