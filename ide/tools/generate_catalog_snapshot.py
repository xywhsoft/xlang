"""Generate an IDE catalog from the matching current release and frozen sources."""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,tarfile
from pathlib import Path
from catalog_sources import plain,sha,verify

IDE=Path(__file__).resolve().parents[1]
DOCS=['语言基础设计','函数设计','表达式与控制流设计','类型系统设计','扩展库系统设计','标准库设计']
PARSER_SOURCES=['src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
 'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c','src/ast/xl6_ast.c','src/diag/xl6_diag.c',
 'src/common/xl6_output.c','src/common/xl6_console.c','src/sem/xl6_completion.c','src/type/xl6_type_model.c']
TOOLS=['generate_catalog.py','catalog_sources.py','dump_stdlib_catalog.c','catalog_inventory.py']

def probe(raw):
    # References are only parsed/expanded, never checked or executed. Let the
    # actual expander choose conditional declarations and optional members.
    chunks='\n'.join(row['source'] for row in raw['sourceChunks'])
    types=sorted(set(re.findall(r'(?m)^\s*(?:public\s+)?(?:class|protocol|enum)\s+(\w+)',chunks)))
    members=sorted(set(re.findall(r'\b(?:func|get)\s+(\w+)',chunks)))
    lines=['import '+ns for ns in sorted({row['namespace'] for row in raw['functions'] if row['namespace'] and not row['namespace'].startswith('__')})]
    lines += ['auto __catalog_type_'+str(i)+' = '+name+'()' for i,name in enumerate(types)]
    lines += ['auto __catalog_member_'+str(i)+' = __catalog_receiver.'+name for i,name in enumerate(members)]
    lines += ['auto __catalog_function_'+str(i)+' = '+(row['namespace']+'.' if row['namespace'] else '')+row['name']+'()' for i,row in enumerate(raw['functions'])]
    return '\n'.join(lines)+'\n'

def generate(root,output,release_report,gcc):
    root=Path(root).resolve();output=Path(output).absolute();release_report=Path(release_report).resolve()
    if output.exists():raise ValueError('Use a fresh output directory')
    evidence=json.loads(release_report.read_text(encoding='utf-8-sig'))
    if not evidence.get('releaseReady'):raise ValueError('Current release acceptance must pass first')
    revision=evidence['sourceRevisions']['windowsArchive']
    # Concurrent IDE work is outside the compiler input boundary. A compiler
    # source edit requires a new matched build, not merely a stale snapshot.
    subprocess.run(['git','diff','--quiet',revision,'--','src','lib','include','release/include'],cwd=root,check=True)
    expected={Path(row['path']).name:row['sha256'] for row in evidence['products']}
    compiler_files={name:sha(root/'release'/name) for name in ('xl.exe','xl.dll','xlc.dll')}
    if any(expected.get(name)!=value for name,value in compiler_files.items()):raise ValueError('Release products differ from their build acceptance')
    output.mkdir(parents=True);snapshot=output/'snapshot/compiler';snapshot.mkdir(parents=True)
    archive=output/'compiler-source.tar'
    subprocess.run(['git','archive','--format=tar','--output='+str(archive),revision,'src','release/include'],cwd=root,check=True)
    with tarfile.open(archive) as source:source.extractall(snapshot,filter='data')
    captured=[]
    def capture(src,rel):
        plain(src);target=snapshot/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,target)
        captured.append({'source':str(src),'path':Path(rel).as_posix(),'sha256':sha(target)})
    for name in compiler_files:capture(root/'release'/name,Path('release')/name)
    for doc in sorted((root/'docs').glob('*.md')):capture(doc,Path('docs')/doc.name)
    for package in sorted((root/'release/extlibs').glob('*/xlang.package.json')):
        manifest=json.loads(package.read_text(encoding='utf-8-sig'));capture(package,package.relative_to(root))
        for rel in manifest.get('artifact',{}).get('xlSources',[]):
            src=(package.parent/rel).resolve()
            if not src.is_relative_to(package.parent.resolve()):raise ValueError('Package declaration escapes its package')
            capture(src,src.relative_to(root))
        readme=package.parent/'README.md'
        if readme.is_file():capture(readme,readme.relative_to(root))
    tool_snapshot=output/'snapshot/ide/tools';tool_snapshot.mkdir(parents=True)
    for name in TOOLS:shutil.copyfile(IDE/'tools'/name,tool_snapshot/name)
    tool_files={name:sha(tool_snapshot/name) for name in TOOLS}
    scratch=output/'scratch';scratch.mkdir()
    header=(snapshot/'src/stdlib/xl6_stdlib_data.h').read_text(encoding='utf-8-sig')
    chunks=re.findall(r'extern\s+const\s+char\*\s+(xl6Stdlib\w+Source)\s*;',header)
    if not chunks:raise ValueError('No class declaration exports')
    (scratch/'stdlib_catalog_chunks.inc').write_text(''.join('    CHUNK('+name+');\n' for name in chunks),encoding='utf-8')
    compiler=snapshot/'release/xl.exe';exporter=scratch/'dump_stdlib_catalog.exe'
    env=dict(os.environ,PATH=str(snapshot/'release')+os.pathsep+os.environ.get('PATH',''),PYTHONUTF8='1')
    report={'format':'xlang-catalog-generation-2','status':'running','sourceRevision':revision,
      'sourceArchiveSha256':sha(archive),'releaseReportSha256':sha(release_report),'compilerFiles':compiler_files,
      'toolFiles':tool_files,'inputs':captured,'calls':[],'snapshot':str(snapshot)}
    def save():(output/'generation-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    def run(arguments):
        log=output/('generation-'+str(len(report['calls']))+'.log');step={'arguments':list(map(str,arguments)),'log':str(log),'exitCode':None};report['calls'].append(step);save()
        with log.open('wb') as stream:code=subprocess.run(list(map(str,arguments)),cwd=snapshot,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        step.update(exitCode=code,logSha256=sha(log));save()
        if code:raise ValueError('Generation failed: '+str(log))
    save()
    try:
        run([gcc,'-std=c11','-O0','-ffunction-sections','-fdata-sections','-I'+str(snapshot),'-I'+str(snapshot/'release/include'),'-I'+str(scratch),
          tool_snapshot/'dump_stdlib_catalog.c',snapshot/'src/stdlib/xl6_stdlib_data.c',*[snapshot/name for name in PARSER_SOURCES],snapshot/'release/xl.dll','-Wl,--gc-sections','-o',exporter])
        raw=scratch/'stdlib.raw.json';run([exporter,raw]);raw_data=json.loads(raw.read_text(encoding='utf-8'))
        trigger=scratch/'all-stdlib-references.xl';trigger.write_text(probe(raw_data),encoding='utf-8')
        expanded=snapshot/'src/stdlib/catalog.declarations.xl';run([exporter,'--expand',trigger,expanded])
        product_version=snapshot/'release/product-version.json';product_version.write_bytes(subprocess.check_output([str(compiler),'--version','--json'],cwd=snapshot,env=env))
        (snapshot/'release/catalog-binding.json').write_text(json.dumps({'sourceRevision':revision,'compilerFiles':compiler_files,'releaseReportSha256':sha(release_report)},indent=2),encoding='utf-8')
        catalog=output/'commands.json';bundle=output/'catalog-sources'
        run([sys.executable,'-X','utf8',tool_snapshot/'generate_catalog.py','--root',snapshot,'--raw',raw,'--compiler',compiler,'--output',catalog,'--source-output',bundle,'--scratch',scratch/'asts'])
        if any(sha(snapshot/row['path'])!=row['sha256'] for row in captured):raise ValueError('Snapshot changed during generation')
        if any(sha(tool_snapshot/name)!=value for name,value in tool_files.items()):raise ValueError('Generator snapshot changed')
        if any(sha(root/'release'/name)!=value for name,value in compiler_files.items()):raise ValueError('Compiler products changed during generation')
        data=json.loads(catalog.read_text(encoding='utf-8'));material=verify(bundle,catalog)
        if material['unavailable']:raise ValueError('Missing declaration sources')
        report.update(status='passed',catalogSha256=sha(catalog),sourceIndexSha256=sha(bundle/'index.json'),commands=len(data['entries']),documents=len(data['documents']),availableSources=material['available'],unavailableSources=0)
    finally:
        if report['status']=='running':report['status']='not-passed'
        save()
    return output

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,default=IDE.parent);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--release-report',type=Path);ap.add_argument('--gcc',default=shutil.which('gcc') or '')
    args=ap.parse_args()
    if not args.gcc:ap.error('gcc is required')
    try:result=generate(args.root,args.output,args.release_report or args.root/'docs/reports/prerelease-0.6.0.json',args.gcc)
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:ap.exit(1,str(error)+'\n')
    print(json.dumps({'status':'catalog-pair-generated','directory':str(result)},ensure_ascii=False))
if __name__=='__main__':main()
