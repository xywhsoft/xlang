from pathlib import Path
import json,hashlib,gzip,sys,re
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
from native_c_interop_profile import PINS,ASYNC_RESULT_PINS
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
seed=read(O/'source-base.json');A=Path(seed['baseline']);b=read(B/'build-result.json');a=read(A/'build-result.json');verify_build(b,B)
aliases=read(O/'baseline-aliases.json')['aliases']
def baseline():
 for p,d in a['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for p,d in a['outputs'].items():assert sha(A/p)==d,p
baseline();assert read(O/'acceptance-chain-v2/result.json')['status']=='completed'
validation=read(O/'validation-v2/result.json');assert validation['status']=='passed'
assert (R/'release/tmp').exists()==validation['rootTmpInitiallyPresent']
for p,d in {**seed['protected'],**validation['protected']}.items():assert sha(p)==d,p
freeze=read(O/'frontend-freeze.json');assert len(freeze['files'])==356
changes=[]
for row in freeze['files']:
 rel=row['path'];assert sha(row['sourcePath'])==row['sourceSha256'],rel
 assert sha(O/'frontend'/rel)==row['preparedSha256'],rel
 if row['preparedSha256']!=PINS[rel]:changes.append(rel);assert row['preparedSha256']==ASYNC_RESULT_PINS[rel]
assert set(changes)==set(ASYNC_RESULT_PINS)
header=Path(b['privateAstConsumers'][0]['header']);assert sha(header)==sha(a['privateAstConsumers'][0]['header'])
faults={n:read(O/n/'result.json') for n in ['final-scalar-faults-v1','canonical-faults-v1','geometry-faults-v1','omissions-v1','ast-replacements-faults-v2','c-interop-faults-v1']}
for n,r in faults.items():
 assert r['status']=='passed',n
 for p,d in r.get('protected',{}).items():assert sha(p)==d,p
wire=read(O/'wire-v1/result.json');assert wire['status']=='passed'
for p,d in wire['protected'].items():assert sha(p)==d,p
for name in ['inputs-current-v2','inputs-previous-v2']:
 result=read(O/name/'result.json');assert result['status']=='passed' and len(result['cases'])==11
 for p,d in result['protected'].items():assert sha(p)==d,p
perf=read(O/'performance-v1/result.json');assert perf['status'] in ['completed','performance-not-admitted'];performance={};hot=[]
for name in ['aa','ab']:
 r=read(O/'performance-v1'/name/'result.json');assert all(p['exit']==0 and not p['cleanupErrors'] for p in r['processes'])
 performance[name]=dict(status=r['status'],queries=len(r['queries']),processes=len(r['processes']),gates=len(r['gates']),failedGates=[g for g in r['gates'] if not g['passed']],minimumCommitGiB=min(x['availableCommit'] for x in r['resources'])/2**30)
 if name=='ab':
  for case,v in r['summary'].items():
   before=v['hot']['baseline']['ms']['p50'];after=v['hot']['candidate']['ms']['p50'];hot.append(dict(case=case,baselineMs=before,candidateMs=after,changePercent=100*(after/before-1)))
S=R/'dev/release-materials/sources/lsp-current-async-frontend-20261103';S.mkdir(parents=True,exist_ok=False);(S/'.gitattributes').write_text('* -text\n');files=[];reports=[]
def snapshot(p,relative):
 p=Path(p);target=S/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(p.read_bytes());assert sha(p)==sha(target)
 files.append(dict(recordedPath=str(p),archivePath=target.relative_to(R).as_posix(),sha256=sha(p),bytes=p.stat().st_size))
owned=['ide/src/analysis/ast_storage.h','ide/tests/analysis_ast_replacement_test.c','ide/tests/analysis_unified_semantic_test.c','ide/tests/fixtures/async-results/async_reused_worker.xl','ide/tools/build_lsp_core.py','ide/tools/check_ast_replacements.py','ide/tools/check_c_interop_frontend.py','ide/tools/check_frontend_inputs.py','ide/tools/check_lsp_unified_frontend.py','ide/tools/native_c_interop_profile.py','ide/tools/native_current_ir_overlay.py','ide/tools/native_frontend_inputs.py','ide/tools/native_semantic_effects_overlay.py']
for name in owned:snapshot(R/name,Path('after')/name)
for original,archived in aliases.items():snapshot(archived,Path('before')/Path(original).relative_to(R))
for row in freeze['files']:snapshot(O/'frontend'/row['path'],Path('frontend')/row['path'])
for p in O.glob('*.py'):snapshot(p,Path('controllers')/p.name)
for folder in ['ownership-red-v1','ownership-red-v2','ownership-red-v3','ast-replacements-faults-v1','ast-replacements-faults-v2','c-interop-faults-v1']:
 for p in (O/folder).rglob('*'):
  if p.is_file() and p.suffix in ['.c','.h','.inc','.py']:snapshot(p,Path('fixtures')/p.relative_to(O))
paths=set()
for p in O.rglob('*.json'):
 if not p.is_relative_to(O/'frontend') and 'baseline-inputs' not in p.relative_to(O).parts and not p.name.startswith('continuation-state'):paths.add(p)
for p in O.rglob('*.log'):
 if p.stat().st_size:paths.add(p)
for p in sorted(paths):
 raw=p.read_bytes();target=S/'reports'/(str(p.relative_to(O))+'.gz');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(target.read_bytes())==raw
 reports.append(dict(recordedPath=str(p),originalSha256=sha(p),originalBytes=len(raw),archivePath=target.relative_to(R).as_posix(),archiveSha256=sha(target),archiveBytes=target.stat().st_size))
report=dict(phase='M7a2b117',date='2026-11-03',status='correctness-passed-comparison-gates-passed' if perf['accepted'] else 'correctness-passed-performance-not-admitted',accepted=False,sourceBase=seed['head'],scope='Current compiler async factory/callback result semantics, exact source-cohort admission, declaration side-effect observation and pooled native expression replacement ownership; not complete LSP/IDE/performance/release admission.',implementation=dict(sourceRevision='async-results-20261103',originalInputs=356,changedOriginalInputs=changes,astAbiUnchanged=True,originalLanguageProducersRetained=True,asyncMutationObservations=3,actualReplacementOwnershipSeams=4,helperAllocations=0,publicAbiChanged=False),build=dict(path=str(B),reportSha256=sha(B/'build-result.json'),status=b['status'],translationUnits=sum('-c' in c for c in b['commands']),nativePrograms=len(b['runs']),privateAstConsumers=len(b['privateAstConsumers']),inputs=len(b['inputs']),outputs=len(b['outputs']),compilerSha256=b['compilerSha256'],headerSha256=sha(header)),baseline=dict(path=str(A),reportSha256=sha(A/'build-result.json'),inputs=len(a['inputs']),outputs=len(a['outputs']),archivedChangedInputs=[dict(originalPath=p,archivedPath=q,sha256=a['inputs'][p]) for p,q in aliases.items()]),correctness=dict(unifiedChecks=int(re.search(r'passed:\s*(\d+) checks',(B/'analysis_unified_semantic_test-run.log').read_text()).group(1)),replacementPoolReleases=int(re.search(r'passed:\s*(\d+) valid',(B/'analysis_ast_replacement_test-run.log').read_text()).group(1)),freshCompleteAstIndexIrAndRetirement=True,mixedRevisionAndEolChecks=22,faults={n:dict(cases=len(r.get('cases',r.get('omissions',[]))),commands=len(r['commands'])) for n,r in faults.items()},protocolSuites=56,validationCommands=len(validation['commands']),wireSummary={k:wire[k] if k=='status' else len(wire[k]) for k in ['status','processes','checks','navigation','completions']}),performance=performance,hotP50=hot,history=dict(firstOwnershipFaultRun='Generic member fixture did not reach explicit global generic replacement;omission returned0,correctly failing acceptance. Exact failed controller retained. Corrected private global generic fixture uses same normal core/header;no product/test input changed;61 unchanged passed command receipts reused after complete identity verification.',fullV1='Intentionally canceled after an additional real native contextual-method ownership bug was reproduced; compiler failure and cancellation reason retained.',redProducers=['ownership-red-v1','ownership-red-v2','ownership-red-v3'],initialRedFixtureLimit='First red run report is retained but its evolving untracked fixture was not copied before later edits;red-v2/red-v3 and the final four real producer omissions have exact retained source.',fastComponents='Private ABI-identical comparison support core used for early four-producer and complete current corpus checks; complete normal build is the authoritative final gate.'),protectedRuntime=seed['protected'],delivery=dict(rootReleaseChanged=False,guiRebuilt=False,guiInputAdmission=False,currentDeliveryIndexChanged=False),remaining=['Confirmed actual member-completion exposes a generated __xl_c_owned_async_* worker;fix native generated declaration visibility and verify async signature presentation.','True local final IR and whole-module validation costs.','Whole ordinary AST materialization and typed index directory production.','All performance,scale,memory and long-run admission.','Matched current compiler/SDK/IDE integration and complete applicable LSP acceptance.'],sourceSnapshots=files,reportArchives=reports)
out=R/'dev/release-materials/provenance/lsp-current-async-frontend-20261103.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');(S/'manifest.json').write_text(json.dumps(dict(phase=report['phase'],files=files,reports=reports),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
verify_build(b,B);baseline()
for p,d in seed['protected'].items():assert sha(p)==d,p
print(json.dumps(dict(report=str(out),build=report['build'],correctness=report['correctness'],performance=performance,snapshots=len(files),archives=len(reports))))
