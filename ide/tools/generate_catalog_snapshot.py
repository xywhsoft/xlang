"""Generate an IDE catalog from the matching current release and frozen sources."""
import argparse,hashlib,json,os,re,shutil,subprocess,sys,tarfile
from pathlib import Path
from catalog_sources import plain,sha,verify
from refresh_extensions import generate as refresh_extensions

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

def compiler_evidence(release_report, compiler_sdk):
    report_bytes=release_report.read_bytes()
    report_digest=hashlib.sha256(report_bytes).hexdigest()
    evidence=json.loads(report_bytes)
    if compiler_sdk is None:
        if not evidence.get('releaseReady'):raise ValueError('Current release acceptance must pass first')
        return (evidence['sourceRevisions']['windowsArchive'],
                {Path(row['path']).name:row['sha256'] for row in evidence['products']},None,report_digest,None)
    sdk_bytes=compiler_sdk.read_bytes()
    sdk=json.loads(sdk_bytes)
    if (not all(evidence.get(key) for key in ('complete','passed','fullTccXrtAndVfs')) or
            evidence.get('optimization')!='O2' or not sdk.get('complete') or not sdk.get('passed') or
            sdk.get('compilerBuildSha256')!=report_digest):
        raise ValueError('The compiler SDK must match a completed full O2 build')
    freeze=Path(evidence['sourceFreeze']).read_bytes()
    if hashlib.sha256(freeze).hexdigest()!=evidence['sourceFreezeSha256']:
        raise ValueError('The compiler source freeze has changed')
    snapshot=json.loads(freeze)
    products={name:row['sha256'] for name,row in evidence['products'].items()}
    return snapshot['xlangCommit'],products,sdk['files'],report_digest,hashlib.sha256(sdk_bytes).hexdigest()


def generate(root,output,release_report,gcc,release_directory=None,compiler_sdk=None):
    root=Path(root).resolve();output=Path(output).absolute();release_report=Path(release_report).resolve()
    if output.exists():raise ValueError('Use a fresh output directory')
    release=Path(release_directory).resolve() if release_directory else root/'release/win_x64'
    if not release_directory and not (release/'xl.exe').is_file():release=root/'release'
    compiler_sdk=Path(compiler_sdk).resolve() if compiler_sdk else None
    revision,expected,sdk_files,report_digest,sdk_digest=compiler_evidence(release_report,compiler_sdk)
    if not re.fullmatch('[0-9a-f]{40}',revision):raise ValueError('Expected an exact compiler source commit')
    # Concurrent IDE work is outside the compiler input boundary. A compiler
    # source edit requires a new matched build, not merely a stale snapshot.
    subprocess.run(['git','diff','--quiet',revision,'--','src','lib','include','release/include'],cwd=root,check=True)
    compiler_files={name:sha(release/name) for name in ('xl.exe','xl.dll','xlc.dll')}
    if any(expected.get(name)!=value for name,value in compiler_files.items()):raise ValueError('Release products differ from their build acceptance')
    headers=sorted(path for path in (release/'include').rglob('*') if path.is_file())
    if not headers:raise ValueError('The compiler release has no public headers')
    header_hashes={p.relative_to(release).as_posix():sha(p) for p in headers}
    if sdk_files is not None:
        consumed=compiler_files | header_hashes
        if any(sdk_files.get(name)!=digest for name,digest in consumed.items()):
            raise ValueError('Compiler products or headers differ from their staged SDK')
        if any(name.startswith('include/') and name not in consumed for name in sdk_files):
            raise ValueError('Compiler headers are missing from the selected release directory')
    output.mkdir(parents=True);snapshot=output/'snapshot/compiler';snapshot.mkdir(parents=True)
    archive=output/'compiler-source.tar'
    subprocess.run(['git','archive','--format=tar','--output='+str(archive),revision,'src'],cwd=root,check=True)
    with tarfile.open(archive) as source:source.extractall(snapshot,filter='data')
    captured=[]
    def capture(src,rel,expected=None):
        plain(src);target=snapshot/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(src,target)
        digest=sha(target)
        if digest!=(expected if expected is not None else sha(src)):
            raise ValueError('Source changed while taking the catalog snapshot: '+str(src))
        captured.append({'source':str(src),'path':Path(rel).as_posix(),'sha256':digest})
    for name,digest in compiler_files.items():capture(release/name,Path('release')/name,digest)
    for name,digest in header_hashes.items():capture(release/name,Path('release')/name,digest)
    for doc in sorted((root/'docs').glob('*.md')):capture(doc,Path('docs')/doc.name)
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
      'sourceArchiveSha256':sha(archive),'releaseReportSha256':report_digest,'compilerFiles':compiler_files,
      'toolFiles':tool_files,'inputs':captured,'calls':[],'snapshot':str(snapshot)}
    if compiler_sdk:report['compilerSdkSha256']=sdk_digest
    def save():(output/'generation-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    def run(arguments):
        log=output/('generation-'+str(len(report['calls']))+'.log');step={'arguments':list(map(str,arguments)),'log':str(log),'exitCode':None};report['calls'].append(step);save()
        with log.open('wb') as stream:code=subprocess.run(list(map(str,arguments)),cwd=snapshot,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        step.update(exitCode=code,logSha256=sha(log));save()
        if code:raise ValueError('Generation failed: '+str(log))
    save()
    try:
        run([gcc,'-std=c11','-O0','-ffunction-sections','-fdata-sections','-I'+str(snapshot),'-I'+str(snapshot/'release'),'-I'+str(snapshot/'release/include'),'-I'+str(scratch),
          tool_snapshot/'dump_stdlib_catalog.c',snapshot/'src/stdlib/xl6_stdlib_data.c',*[snapshot/name for name in PARSER_SOURCES],snapshot/'release/xl.dll','-Wl,--gc-sections','-o',exporter])
        raw=scratch/'stdlib.raw.json';run([exporter,raw]);raw_data=json.loads(raw.read_text(encoding='utf-8'))
        trigger=scratch/'all-stdlib-references.xl';trigger.write_text(probe(raw_data),encoding='utf-8')
        expanded=snapshot/'src/stdlib/catalog.declarations.xl';run([exporter,'--expand',trigger,expanded])
        product_version=snapshot/'release/product-version.json';product_version.write_bytes(subprocess.check_output([str(compiler),'--version','--json'],cwd=snapshot,env=env))
        (snapshot/'release/catalog-binding.json').write_text(json.dumps({'sourceRevision':revision,'compilerFiles':compiler_files,'releaseReportSha256':report_digest},indent=2),encoding='utf-8')
        standard=output/'standard';standard.mkdir()
        catalog=standard/'commands.json';bundle=standard/'catalog-sources'
        run([sys.executable,'-X','utf8',tool_snapshot/'generate_catalog.py','--root',snapshot,'--raw',raw,'--compiler',compiler,'--output',catalog,'--source-output',bundle,'--scratch',scratch/'asts'])
        complete=output/'complete'
        report['extensionGeneration']=refresh_extensions(root,standard,complete,gcc,release)
        catalog=output/'commands.json';bundle=output/'catalog-sources'
        shutil.copy2(complete/'commands.json',catalog)
        shutil.copytree(complete/'catalog-sources',bundle)
        if any(sha(snapshot/row['path'])!=row['sha256'] for row in captured):raise ValueError('Snapshot changed during generation')
        if any(sha(tool_snapshot/name)!=value for name,value in tool_files.items()):raise ValueError('Generator snapshot changed')
        if any(sha(Path(row['source']))!=row['sha256'] for row in captured):raise ValueError('Catalog input sources changed during generation')
        if sha(release_report)!=report_digest or (compiler_sdk and sha(compiler_sdk)!=sdk_digest):
            raise ValueError('Catalog build receipts changed during generation')
        data=json.loads(catalog.read_text(encoding='utf-8'));material=verify(bundle,catalog)
        if material['unavailable']:raise ValueError('Missing declaration sources')
        report.update(status='passed',catalogSha256=sha(catalog),sourceIndexSha256=sha(bundle/'index.json'),commands=len(data['entries']),documents=len(data['documents']),availableSources=material['available'],unavailableSources=0)
    finally:
        if report['status']=='running':report['status']='not-passed'
        save()
    return output

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,default=IDE.parent);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--release-report',type=Path);ap.add_argument('--gcc',default=shutil.which('gcc') or '')
    ap.add_argument('--release-directory',type=Path,help='Windows x64 runtime containing the matched compiler and headers')
    ap.add_argument('--compiler-sdk',type=Path,help='SDK staging receipt paired with a full compiler build report')
    args=ap.parse_args()
    if not args.gcc:ap.error('gcc is required')
    if args.compiler_sdk and not args.release_report:ap.error('--compiler-sdk requires --release-report to name its full build report')
    try:result=generate(args.root,args.output,args.release_report or args.root/'docs/reports/prerelease-0.6.0.json',
                        args.gcc,args.release_directory,args.compiler_sdk)
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:ap.exit(1,str(error)+'\n')
    print(json.dumps({'status':'catalog-pair-generated','directory':str(result)},ensure_ascii=False))
if __name__=='__main__':main()
