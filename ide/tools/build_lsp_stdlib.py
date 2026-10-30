"""Qualify the normal current stdlib producer against original expansion.

Uses a verified native semantic archive and the actual memo/query support.
No transport,IDE,release installation or complete-service admission is implied.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys,time
from build_lsp_core import dependencies,prepare_native_stdlib,prepare_native_sem,prepare_native_completion
from native_context_overlay import completion
from native_current_stdlib_profile import profile

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--base-source-revision',help='Git revision for exact historical controller inputs only')
    args=ap.parse_args();repo=Path(__file__).resolve().parents[2];base=args.base.resolve(strict=True)
    old=json.loads((base/'build-result.json').read_text(encoding='utf8'));assert old['status']=='passed'
    front=Path(old['sourceOverlay']['source']).parents[2];archive=base/'xlang-semantic-substrate.a';assert sha(archive)==old['outputs'][archive.name]
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);(out/'frontend').mkdir()
    report=dict(status='running',accepted=False,scope='normal current stdlib producer,real original oracle and raw hooks; no full service admission',commands=[],inputs=profile(front),outputs={},baseReceipt=str(base/'build-result.json'),baseReceiptSha256=sha(base/'build-result.json'),baseArchiveSha256=sha(archive),historicalBaseInputs={},overlays=[])
    for p,h in old['inputs'].items():
     if sha(p)==h:continue
     if not args.base_source_revision or not Path(p).is_relative_to(repo/'ide/tools'):raise ValueError('base input changed; rebuild or provide its exact controller revision: '+p)
     data=subprocess.check_output(['git','show',args.base_source_revision+':'+Path(p).relative_to(repo).as_posix()],cwd=repo)
     # Historical checkouts may have CRLF controller bytes. Accept only the exact
     # recorded digest; these alternatives never apply to frontend admission.
     variants=[data]
     if b'\r\n' not in data:variants.append(data.replace(b'\n',b'\r\n'))
     exact=next((v for v in variants if hashlib.sha256(v).hexdigest()==h),None)
     if exact is None:raise ValueError('Git controller does not match recorded base input: '+p)
     match=out/'base-inputs'/Path(p).name;match.parent.mkdir(exist_ok=True);match.write_bytes(exact)
     report['historicalBaseInputs'][p]=str(match)
    command=next(c for c in old['commands'] if '-c' in c and Path(c[c.index('-c')+1]).name=='xl6_stdlib.c');flags=command[:command.index('-c')];cc=flags[0];env=dict(os.environ);env['PATH']=str(Path(cc).parent)+os.pathsep+env.get('PATH','');flags[1:1]=['-I'+str(repo/'ide/tests')]
    start=time.monotonic()
    def publish():(out/'build-result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8',newline='\n')
    def run(args,label):
     report['commands'].append(args);p=subprocess.run(args,cwd=repo,env=env,capture_output=True,timeout=300);(out/(label+'.log')).write_bytes(p.stdout+p.stderr);publish();assert p.returncode==0,(label,p.returncode,p.stderr.decode('utf8'));return p
    try:
     assets=[prepare_native_stdlib(front,out),prepare_native_sem(front,out)];_,ns,nsm=completion(front,out);assets.append(prepare_native_completion(front,out,ns,_current_unit_probe=True))
     report['overlays']=[a[2] for a in assets]
     for a in assets:report['inputs'][str(a[0])]=sha(a[0]);report['inputs'].update(a[2].get('auditedDependencies',{}))
     report['inputs'].update(nsm['auditedDependencies'])
     signature=out/'signature_query.c';signature.write_text('#include "analysis/signature_internal.h"\n#include "analysis/frontend_internal.h"\n#include "analysis/frontend_raw_body.h"\n#include "analysis/frontend_completion_path.h"\n#include <stdio.h>\n#include <string.h>\n#include "analysis/signature_data.inc"\n#include "analysis/signature_parse.inc"\n',encoding='utf8',newline='\n')
     reference=out/'reference_stdlib.c';reference.write_bytes((front/'src/stdlib/xl6_stdlib.c').read_bytes());report['unmodifiedStdlibOracle']=dict(source=str(front/'src/stdlib/xl6_stdlib.c'),copy=str(reference),sha256=sha(reference))
     sources=[a[1] for a in assets]+[signature,*[repo/'ide/src/analysis'/n for n in ['frontend_context.c','frontend_raw_body.c','frontend_symbols.c']],reference,repo/'ide/tests/analysis_stdlib_current_test.c']
     for p in [Path(__file__),Path(cc),archive,*[Path(m.__file__).resolve() for m in list(sys.modules.values()) if getattr(m,'__file__',None) and Path(m.__file__).resolve().parent==repo/'ide/tools']]:report['inputs'][str(p)]=sha(p)
     objects=[]
     for source in sources:
      sf=flags+(['-Dxl6StdlibExpandModule=xlsReferenceStdlibExpandModule','-Dxl6StdlibImplicitMethodInternalName=xlsReferenceStdlibImplicitMethodInternalName'] if source==reference else [])
      dep=run(sf+['-MM','-MT','xlsObject',str(source)],source.stem+'-dependencies');report['inputs'].update({str(p):sha(p) for p in dependencies(dep.stdout.decode('utf8'))})
      obj=out/(source.stem+'.o');run(sf+['-c',str(source),'-o',str(obj)],source.stem);objects.append(obj);report['outputs'][obj.name]=sha(obj);publish()
     program=out/('stdlib-test.exe' if os.name=='nt' else 'stdlib-test');libs=['-lm']+(['-lws2_32','-lwinhttp','-liphlpapi','-lgdi32','-luser32','-lshell32','-lole32','-lwinmm','-lavrt','-lversion','-lbcrypt','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl'])
     nm=Path(cc).with_name('nm.exe' if os.name=='nt' else 'nm');report['inputs'][str(nm)]=sha(nm)
     exports=run([str(nm),'--defined-only','--extern-only',str(out/'reference_stdlib.o')],'reference-exports').stdout.decode('utf8');names=[line.split()[-1] for line in exports.splitlines() if len(line.split())==3 and line.split()[1]=='T'];assert sorted(names)==['xlsReferenceStdlibExpandModule','xlsReferenceStdlibImplicitMethodInternalName'];report['referenceExports']=names
     run([cc,*map(str,objects),str(archive),'-Wl,--gc-sections',*libs,'-o',str(program)],'link');p=run([str(program)],'run');assert b'analysis_stdlib_current_test passed:' in p.stdout,p.stdout.decode('utf8')
     for raw,h in report['inputs'].items():assert sha(raw)==h,raw
     report['outputs'][program.name]=sha(program);report['status']='passed';print(p.stdout.decode('utf8'))
    except BaseException as e:report.update(status='failed',error=repr(e));raise
    finally:report['durationSeconds']=time.monotonic()-start;publish()

if __name__=='__main__':main()
