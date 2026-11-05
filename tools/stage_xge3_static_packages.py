"""Prepare a separate XGE/XUI package with a direct-XRT static native archive.

Existing direct methods and policies are copied, not wrapped. Selected XRT
modules come from actual archive undefined symbols and the compiler's exact
catalog; multiple module witnesses for one symbol are a conjunction.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','engine-root','engine-build','out','report'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--bootstrap-only',action='store_true')
    p.add_argument('--include-shell-resource',action='store_true',help='Opt in to the XGE DLL-shell icon, not required engine data')
    a=p.parse_args();root,native,build,out,report=[v.resolve()for v in(a.root,a.engine_root,a.engine_build,a.out,a.report)]
    assert not out.exists()and not report.exists()and not report.is_relative_to(out)
    assert not out.is_relative_to(root/'release')and not report.is_relative_to(root/'release')
    metadata=json.loads((build/'build-report.json').read_text(encoding='utf-8'))
    assert metadata['complete']and metadata['passed']and metadata['sourceUnchanged']
    assert metadata['linkage']=='static-archive'and metadata['xrt_mode']=='direct-external'
    assert not metadata['xrt_implementation']and not metadata['host_xrt_import_slots']
    classification=None
    if metadata['platform']=='win_x64':
        classification=metadata.get('compiler_support',{}).get('importClassification')
        assert classification and not classification['unknownInventory'],'Windows import kind is not proven'
        for path,digest in classification['dllInputs'].items():
            assert sha(path)==digest,'System DLL changed after import classification: '+path
        for library in metadata['compiler_support']['archives']:
            assert all(row.get('kindKnown')and row['kind']in('function','data')
                for row in library['importMembers']),'Import kind must be explicit, never guessed'
    assert Path(metadata['source_root']).resolve()==native
    canonical=hashlib.sha256((root/'lib/xrt/xrt.h').read_bytes().replace(b'\r\n',b'\n')).hexdigest()
    assert canonical==metadata['xrt_header_sha256'],'Native and language XRT contracts differ'
    # The current common policy source needs this complete declaration surface.
    # Reduced profiles are valid C SDKs, not silently equivalent language SDKs.
    required={'XGE_ENABLE_XUI','XGE_ENABLE_TEXT','XGE_ENABLE_AUDIO','XGE_ENABLE_SHAPE_EX'}
    assert required.issubset(metadata['features']),'The common policy source needs full XUI/text/audio/Shape declarations'
    archive=build/metadata['archive'];assert sha(archive)==metadata['archive_sha256']
    catalog=root/'src/compile/xl6_artifact_native_symbols.inc';mapping={}
    for name,module in re.findall(r'XL6_ARTIFACT_NATIVE_SYMBOL\("([^"\n]+)",\s*(NULL|"[^"\n]+")\)',catalog.read_text(encoding='utf-8')):
        mapping.setdefault(name,set())
        if module!='NULL':mapping[name].add(module.strip('"'))
    symbols=metadata['xrt_undefined_symbols'];unknown=sorted(set(symbols)-set(mapping))
    assert not unknown,'Unmapped XRT references: '+str(unknown)
    modules=sorted(set().union(*(mapping[name]for name in symbols)))
    features=set(metadata['features']);packages=['_xge_native']if a.bootstrap_only else['_xge_native','xge','xui']
    if not a.bootstrap_only and 'XGE_ENABLE_3D'in features:packages.append('xge3d')
    if not a.bootstrap_only and metadata['debug_api']:packages.append('xgeDebug')
    sources=root/'lib/bindings';source_paths={name:sources/({'xge3d':'xge/three_d','xgeDebug':'xge/debug'}.get(name,name))for name in packages}
    inputs={str(v):sha(v)for v in [Path(__file__).resolve(),catalog,root/'lib/xrt/xrt.h',build/'build-report.json',archive]}
    if classification:inputs.update(classification['dllInputs'])
    headers=list((native/'include').glob('*.h'))
    for path in [*headers,native/'extlibs/xrt/xrt_config.h',build/'xge_build_config.h',build/'xge_static_xrt_contract.h']:inputs[str(path)]=sha(path)
    for name,folder in source_paths.items():
        for path in folder.rglob('*'):
            if path.is_file()and not(name=='xge'and path.relative_to(folder).parts[0]in('three_d','debug')):inputs[str(path)]=sha(path)
    out.mkdir(parents=True)
    for name,folder in source_paths.items():shutil.copytree(folder,out/name,ignore=shutil.ignore_patterns('three_d','debug')if name=='xge'else None)
    private=out/'_xge_native';target=private/'include';native_dir=private/'native';native_dir.mkdir()
    for path in [*headers,build/'xge_build_config.h',build/'xge_static_xrt_contract.h']:shutil.copy2(path,target/path.name)
    config=target/'xge_build_config.h';content=config.read_text(encoding='utf-8');guard='#if !defined(XL6_NATIVE_LINK_SCHEMA) || XL6_NATIVE_LINK_SCHEMA != 1\n#error "Static XGE requires matching native-link compiler schema 1"\n#endif\n';config.write_text(guard+content,encoding='utf-8',newline='\n')
    xrt=target/'extlibs/xrt';xrt.mkdir(parents=True,exist_ok=True);shutil.copy2(native/'extlibs/xrt/xrt_config.h',xrt/'xrt_config.h')
    (xrt/'xrt.h').write_text('/* Sole program XRT declarations; no private implementation. */\n#include <xrt_decl.h>\n',encoding='utf-8',newline='\n')
    shutil.copy2(archive,native_dir/archive.name);links=['native/'+archive.name]
    if a.include_shell_resource and metadata.get('resource'):
        path=build/metadata['resource'];assert sha(path)==metadata['resource_sha256'];inputs[str(path)]=sha(path);shutil.copy2(path,native_dir/path.name);links.append('native/'+path.name)
    support=metadata.get('compiler_support')
    assert support,'Static package requires audited compiler support'
    if support:
        for row in support['artifacts']:
            path=Path(row['path']);assert sha(path)==row['sha256'];inputs[str(path)]=sha(path);shutil.copy2(path,native_dir/path.name);links.append('native/'+path.name)
        notices=private/'notices/compiler-support';notices.mkdir(parents=True,exist_ok=True)
        for row in support['notices']:
            path=Path(row['path']);assert sha(path)==row['sha256'];inputs[str(path)]=sha(path);shutil.copy2(path,notices/path.name)
    for row in metadata.get('external_link_libraries',[]):
        path=Path(row['path']);assert sha(path)==row['sha256'];inputs[str(path)]=sha(path);shutil.copy2(path,native_dir/path.name);links.append('native/'+path.name)
    for row in metadata.get('runtime_dependencies',[]):
        path=Path(row['path']);assert sha(path)==row['sha256'];inputs[str(path)]=sha(path);shutil.copy2(path,native_dir/path.name)
    manifest_path=private/'xlang.package.json';manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    manifest['links']=links+([]if metadata['platform']=='win_x64'else metadata['system_libraries']);manifest['loadlibs']=[];manifest['artifact']['xrtModules']=modules
    manifest['artifact']['runtimeSymbols']=[name for name in manifest['artifact'].get('runtimeSymbols',[])if name!='xl6HostXrtResolveV1']
    assert set(manifest['artifact']['runtimeSymbols'])=={'xl6ErrorScopeTryPropagateFailureV1','xl6CallbackFailureRecordV1','xl6ForeignStatusFailureV1'}
    manifest['artifact']['nativeLink']=dict(gcSections=True,functionSections=True,dataSections=True)
    manifest['native'].update(loadName='xge-static-api3',platform=metadata['platform'],linkage='static',sha256=sha(archive))
    manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    assert all(sha(path)==digest for path,digest in inputs.items()),'A staging input changed'
    outputs={str(path.relative_to(out)):sha(path)for path in sorted(out.rglob('*'))if path.is_file()}
    result=dict(complete=True,passed=True,scope=__doc__,packages=packages,platform=metadata['platform'],engineBuild=str(build/'build-report.json'),
        archive=str(archive),archiveSha256=sha(archive),directXrtSymbols=symbols,xrtModules=modules,xrtModuleWitnesses={name:sorted(mapping[name])for name in symbols},
        inputs=inputs,outputs=outputs,loadlibs=[],runtimeSymbols=manifest['artifact']['runtimeSymbols'],
        resourceCost='Optional XGE DLL-shell icon explicitly selected'if a.include_shell_resource else None,compilerSupport=support,
        runtimeDependencies=metadata.get('runtime_dependencies',[]),shippingDelivery=False,
        limitations=['single/xge.c remains an aggregate; final reachability is per compiler section.',
            'No selected feature is removed; staging runs no GPU/window/browser/OS clipboard action.',
            'Three common resident error/callback requirements remain explicit; per-policy source splitting is a separate optimization.'])
    report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(dict(out=str(out),files=len(outputs),xrtSymbols=len(symbols),xrtModules=len(modules))))

if __name__=='__main__':main()
