"""Generate a complete catalog/source pair from frozen compiler materials."""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,uuid
from pathlib import Path
from catalog_sources import plain,sha,verify

IDE=Path(__file__).resolve().parents[1]
DOCS=['语言基础设计','函数设计','表达式与控制流设计','类型系统设计','扩展库系统设计','标准库设计']

def relative(name):
    parts=name.split('/')
    if any(not p or p in ('.','..') or '\\' in p or ':' in p for p in parts): raise ValueError('Invalid material path: '+name)
    return Path(*parts)

def generate(output,documentation_root,profile_path,gcc):
    output=Path(output).absolute()
    if output.exists(): raise ValueError('Catalog output directory already exists')
    profile_path=Path(profile_path).absolute(); plain(profile_path)
    profile=json.loads(profile_path.read_text(encoding='utf-8-sig'))
    source_manifest=IDE/relative(profile['compiler_source_manifest']); runtime_manifest=IDE/relative(profile['compiler_manifest'])
    if sha(source_manifest)!=profile['compiler_source_manifest_sha256'] or sha(runtime_manifest)!=profile['compiler_manifest_sha256']:
        raise ValueError('Pinned compiler material manifest changed')
    sources=json.loads(source_manifest.read_text(encoding='utf-8-sig')); runtime=json.loads(runtime_manifest.read_text(encoding='utf-8-sig'))
    evidence_path=IDE/relative(profile['compiler_source_build_report'])
    if sha(evidence_path)!=profile['compiler_source_build_report_sha256']: raise ValueError('Compiler source build evidence changed')
    evidence=json.loads(evidence_path.read_text(encoding='utf-8-sig'))
    if evidence.get('status')!='independent-build-passed' or evidence.get('sourceManifestSha256')!=sha(source_manifest): raise ValueError('Compiler source build evidence is not matched')
    compiler_root=IDE/relative(profile['compiler_root']); source_root=Path(sources['directory'])
    if source_root.resolve()!=compiler_root.parent.resolve() or Path(runtime['directory']).resolve()!=source_root.resolve():
        raise ValueError('Compiler source and runtime roots differ')
    rows={row['path']:row['sha256'] for row in sources['files'] if row['path'].startswith(('src/','lib/'))}
    rows.update({name:digest for name,digest in runtime['files'].items() if name.startswith('release/extlibs/')})
    compiler_files={name:runtime['files']['release/'+name] for name in ('xl.exe','xl.dll','xlc.dll')}
    if any(evidence['runtimeArtifacts'].get(name)!=digest for name,digest in compiler_files.items()): raise ValueError('Compiler binaries do not match their source build')
    for name,digest in compiler_files.items():
        plain(compiler_root/name)
        if sha(compiler_root/name)!=digest: raise ValueError('Pinned compiler runtime changed: '+name)
    output.mkdir(parents=True); snapshot=output/'snapshot/demo6'; snapshot.mkdir(parents=True)
    captured=[]
    def capture(source,relative_path,digest=None):
        plain(source); raw=source.read_bytes(); actual=hashlib.sha256(raw).hexdigest()
        if digest and actual!=digest: raise ValueError('Pinned source material changed: '+str(source))
        target=snapshot/relative_path; target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(raw)
        captured.append({'source':str(source),'path':relative_path.as_posix(),'sha256':actual})
    for name,digest in sorted(rows.items()): capture(source_root/relative(name),relative(name),digest)
    for name in DOCS: capture(Path(documentation_root)/(name+'.md'),Path('docs')/(name+'.md'))
    tool_snapshot=output/'snapshot/ide/tools'; tool_snapshot.mkdir(parents=True)
    tool_files={}
    for name in ('generate_catalog.py','catalog_sources.py','dump_stdlib_catalog.c'):
        source=IDE/'tools'/name; plain(source); raw=source.read_bytes(); target=tool_snapshot/name; target.write_bytes(raw); tool_files[name]=hashlib.sha256(raw).hexdigest()
    scratch=output/'scratch'; scratch.mkdir()
    header=(snapshot/'src/stdlib/xl6_stdlib_data.h').read_text(encoding='utf-8-sig')
    chunks=re.findall(r'extern\s+const\s+char\*\s+(xl6Stdlib\w+Source)\s*;',header)
    if not chunks: raise ValueError('No class source exports found')
    (scratch/'stdlib_catalog_chunks.inc').write_text(''.join('    CHUNK('+name+');\n' for name in chunks),encoding='utf-8')
    exporter=scratch/'dump_stdlib_catalog.exe'
    compiler=compiler_root/'xl.exe'; catalog=output/'commands.json'; bundle=output/'catalog-sources'
    calls=[[gcc,'-std=c11','-O0','-I'+str(snapshot),'-I'+str(scratch),str(tool_snapshot/'dump_stdlib_catalog.c'),str(snapshot/'src/stdlib/xl6_stdlib_data.c'),'-o',str(exporter)],
           [str(exporter),str(scratch/'stdlib.raw.json')],
           [sys.executable,'-X','utf8',str(tool_snapshot/'generate_catalog.py'),'--demo6',str(snapshot),'--raw',str(scratch/'stdlib.raw.json'),'--compiler',str(compiler),'--output',str(catalog),'--source-output',str(bundle),'--scratch',str(scratch/'asts')]]
    report={'format':'xlang-catalog-generation-1','status':'running','output':str(output),'snapshot':str(snapshot),'sourceManifestSha256':sha(source_manifest),'runtimeManifestSha256':sha(runtime_manifest),'compilerFiles':compiler_files,'toolFiles':tool_files,'inputs':captured,'calls':[]}
    report_path=output/'generation-report.json'
    def receipt(): report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    receipt()
    try:
        for n,arguments in enumerate(calls):
            log=output/(f'generation-{n}.log'); step={'arguments':arguments,'log':str(log),'exitCode':None}; report['calls'].append(step); receipt()
            with log.open('xb') as stream:
                code=subprocess.run(arguments,stdout=stream,stderr=subprocess.STDOUT,timeout=900,cwd=IDE).returncode
            step.update(exitCode=code,logSha256=sha(log)); receipt()
            if code: raise ValueError('Catalog generation failed; inspect '+str(log))
        for row in captured:
            if sha(snapshot/row['path'])!=row['sha256']: raise ValueError('Generation snapshot was modified')
        for name,digest in tool_files.items():
            if sha(tool_snapshot/name)!=digest: raise ValueError('Generation tool snapshot changed')
        for name,digest in compiler_files.items():
            if sha(compiler_root/name)!=digest: raise ValueError('Compiler changed during generation')
        material=verify(bundle,catalog)
        if material['unavailable']: raise ValueError('Generated catalog is missing source snapshots')
        data=json.loads(catalog.read_text(encoding='utf-8')); report.update(status='passed',catalogSha256=sha(catalog),sourceIndexSha256=sha(bundle/'index.json'),commands=len(data['entries']),documents=len(data['documents']),availableSources=material['available'],unavailableSources=0)
    finally:
        if report['status']=='running': report['status']='not-passed'
        receipt()
    return output

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--documentation-root',type=Path,default=IDE.parent/'demo6/docs')
    parser.add_argument('--profile',type=Path,default=IDE/'data/portable-build-inputs.json')
    parser.add_argument('--gcc',default=shutil.which('gcc') or '')
    args=parser.parse_args()
    if not args.gcc: parser.error('gcc is required')
    output=args.output or IDE/'build'/('cg-'+uuid.uuid4().hex[:8])
    print('Generating immutable catalog pair:',output,flush=True)
    try: result=generate(output,args.documentation_root,args.profile,args.gcc)
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error: parser.exit(1,str(error)+'\n')
    print(json.dumps({'status':'catalog-pair-generated','directory':str(result)},ensure_ascii=False))

if __name__=='__main__': main()
