from pathlib import Path
import json,hashlib,gzip,re,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text(encoding='utf8'))
seed=read(O/'source-base.json');A=Path(seed['baseline']);b=read(B/'build-result.json');a=read(A/'build-result.json');verify_build(b,B)
aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
def baseline():
 for p,d in a['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for p,d in a['outputs'].items():assert sha(A/p)==d,p
baseline();assert read(O/'acceptance-chain-v1/result.json')['status']=='completed'
v=read(O/'validation-v1/result.json');assert v['status']=='passed'
assert (R/'release/tmp').exists()==v['rootTmpInitiallyPresent']
for p,d in {**seed['protected'],**v['protected']}.items():assert sha(p)==d,p
faults={n:read(O/n/'result.json') for n in ['final-scalar-faults-v1','canonical-faults-v1','geometry-faults-v1','omissions-v1']}
for n,r in faults.items():
 assert r['status']=='passed',n
 for p,d in r.get('protected',{}).items():assert sha(p)==d,p
wire=read(O/'wire-v1/result.json');assert wire['status']=='passed'
for p,d in wire.get('protected',{}).items():assert sha(p)==d,p
rules={p:d for p,d in b['inputs'].items() if p.startswith(b['frontendRoot']+'\\')}
assert rules
for p,d in rules.items():assert a['inputs'][p]==d,p
header=Path(b['privateAstConsumers'][0]['header']);assert sha(header)==sha(a['privateAstConsumers'][0]['header'])
profile=read(O/'profile-v1/result.json');assert profile['status']=='built'
for p,d in {**profile['inputs'],**profile['outputs']}.items():assert sha(aliases.get(p,p))==d,p
lifecycle=read(O/'lifecycle-v1/result.json');assert lifecycle['status']=='passed' and lifecycle['queries']==120 and len(lifecycle['runs'])==8
for run in lifecycle['runs']:assert not run['exit'] and not run['stderrReaderAlive'] and not run['stderrReaderErrors']
review=read(O/'lifecycle-v1/review.json');timings=[]
for row in review['summaries']:
 if row['phase']=='hot' and row['label'] in ['body.semantic','body.index','body.ir-and-finish','worker.total','adapter.total']:timings.append(row)
for n in ['receipt-probe-v1','receipt-probe-v2']:assert read(O/n/'result.json')['status']=='passed'
assert read(O/'dense-red-v2/result.json')['status']=='expected-reuse-failure'
assert read(O/'focused-v1/result.json')['status']=='failed' and read(O/'focused-v2/result.json')['status']=='passed'
alignment=read(O/'alignment-review-v1/review.json');assert alignment['status']=='reviewed-not-aligned'
perf=read(O/'performance-v1/result.json');assert perf['status'] in ['completed','performance-not-admitted']
performance={};hot=[]
for n in ['aa','ab']:
 r=read(O/'performance-v1'/n/'result.json');assert all(p['exit']==0 and not p['cleanupErrors'] for p in r['processes'])
 performance[n]=dict(status=r['status'],queries=len(r['queries']),processes=len(r['processes']),gates=len(r['gates']),failedGates=[g for g in r['gates'] if not g['passed']],summary=r['summary'],minimumCommitGiB=min(x['availableCommit'] for x in r['resources'])/2**30,minimumPhysicalGiB=min(x['availablePhysical'] for x in r['resources'])/2**30,allProcessesExitZero=True,cleanupErrors=0)
 if n=='ab':
  performance[n]['observedProcessMemory']={variant:{k:max(q['memory'][k] for q in r['queries'] if q['variant']==variant) for k in r['queries'][0]['memory']} for variant in ['baseline','candidate']}
  observed={}
  for case,values in r['summary'].items():
   before=values['hot']['baseline']['ms']['p50'];after=values['hot']['candidate']['ms']['p50'];hot.append(dict(case=case,baselineMs=before,candidateMs=after,changePercent=100*(after/before-1)))
   rows=[q['result']['analysis'].get('lastBuildIndexCanonical') for q in r['queries'] if q['case']==case and q['variant']=='candidate' and q['phase']=='hot'];assert rows and all(x is not None for x in rows)
   observed[case]={k:dict(minimum=min(x[k] for x in rows),maximum=max(x[k] for x in rows)) for k in rows[0]}
  performance[n]['wireCanonicalWork']=observed
S=R/'dev/release-materials/sources/lsp-final-scalar-replay-20261103';S.mkdir(parents=True,exist_ok=False);(S/'.gitattributes').write_text('* -text\n');files=[];reports=[]
def snapshot(p,relative):
 p=Path(p);target=S/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(p.read_bytes());assert sha(p)==sha(target)
 files.append(dict(recordedPath=str(p),archivePath=target.relative_to(R).as_posix(),sha256=sha(p),bytes=p.stat().st_size))
owned=['ide/src/analysis/frontend_sem_final.inc','ide/tests/analysis_final_environment_test.c','ide/tests/analysis_index_canonical_test.c','ide/tools/check_index_canonical.py','ide/tools/check_final_scalar_replay.py','ide/tools/build_lsp_core.py']
for p in owned:snapshot(R/p,Path('after')/p)
for row in seed['archivedInputs']:snapshot(row['archivedPath'],Path('before')/Path(row['originalPath']).relative_to(R))
for p in O.glob('*.py'):snapshot(p,Path('controllers')/p.name)
for p in O.glob('*.c'):snapshot(p,Path('fixtures')/p.name)
for folder in ['receipt-probe-v1/shadow','receipt-probe-v2/shadow','dense-red-v1','dense-red-v2','focused-v1-inputs','profile-v1','final-scalar-faults-v1','canonical-faults-v1','geometry-faults-v1','omissions-v1']:
 for p in (O/folder).rglob('*'):
  if p.is_file() and p.suffix in ['.c','.h','.inc']:snapshot(p,Path('fixtures')/p.relative_to(O))
paths={O/'source-base.json',O/'full-run-v1.json',O/'full-run-v2.json',O/'failed-full-v1-closure.json'}
for p in O.rglob('*.json'):
 if p.name in ['result.json','plan.json','build-result.json','index-overlay.json','review.json','terminal-review.json'] and 'baseline-inputs' not in p.relative_to(O).parts:paths.add(p)
for p in O.rglob('*.log'):
 if p.stat().st_size:paths.add(p)
for p in sorted(paths):
 raw=p.read_bytes();target=S/'reports'/(str(p.relative_to(O))+'.gz');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(target.read_bytes())==raw
 reports.append(dict(recordedPath=str(p),originalSha256=sha(p),originalBytes=len(raw),archivePath=target.relative_to(R).as_posix(),archiveSha256=sha(target),archiveBytes=target.stat().st_size))
def checks(name):return int(re.search(r'passed:\s*(\d+) checks',(B/(name+'-run.log')).read_text()).group(1))
report=dict(phase='M7a2b116',date='2026-11-03',status='correctness-passed-comparison-gates-passed' if perf['accepted'] else 'correctness-passed-performance-not-admitted',accepted=False,sourceBase=seed['head'],scope='Exact current readonly scalar validation permits original final annotation replay after native symbol rebinding;not complete final IR/current frontend/GUI/release admission.',implementation=dict(scalarFields=18,writableFields=9,readonlyFields=9,readonlyUsesOriginalNativeEncoder=True,readonlyNeverWritten=True,unknownAndPackagePayloadMasksRejected=True,originalRegionAndTwoPassBoundariesPreserved=True,changedReadonlyValueUsesOriginalFinalPhases=True,newRetainedCacheBytes=0,canonicalProofGuardsUnchanged=True),build=dict(path=str(B),reportSha256=sha(B/'build-result.json'),status=b['status'],translationUnits=sum('-c' in c for c in b['commands']),nativePrograms=len(b['runs']),privateAstConsumers=len(b['privateAstConsumers']),inputs=len(b['inputs']),outputs=len(b['outputs']),compilerSha256=b['compilerSha256'],headerSha256=sha(header),unchangedFrontendRuleInputs=rules),baseline=dict(path=str(A),reportSha256=sha(A/'build-result.json'),status=a['status'],inputs=len(a['inputs']),outputs=len(a['outputs']),archivedChangedInputs=seed['archivedInputs']),correctness=dict(canonicalAssertions=checks('analysis_index_canonical_test'),finalBoundaryChecks=checks('analysis_final_environment_test'),completeCurrentAstExtendedTypedFactsIrAndSourceDiagnosticReport=True,denseLegacyCompatibilityReportRefusalPreserved=True,denseFunctions=512,denseUnchangedHelpers=511,denseHotFinalAndCanonicalReuseBefore=86,denseHotFinalAndCanonicalReuseAfter=511,denseHotCanonicalCallsBefore=56255,denseHotCanonicalCallsAfter=131,faults={n:dict(cases=len(r['cases']),commands=len(r['commands'])) for n,r in faults.items()},protocolSuites=56,validationCommands=len(v['commands']),wireSummary={k:wire[k] if k=='status' else len(wire[k]) for k in ['status','processes','checks','navigation','completions']}),currentFrontendAlignment=alignment,priorNormalDiagnostics=dict(queries=lifecycle['queries'],processes=len(lifecycle['runs']),timings=timings,scope='Inclusive one-pair M115 diagnosis;parent and child overlap;not M116 performance.',receiptForbiddenMask=24,receiptActualMask=8216,matchingInputOutputAndEnvironmentButNoFinalReplayFunctions=425),performance=performance,hotP50=hot,history=dict(fullV1='All232 translation units compiled;nine native programs passed before the enlarged dense fixture hit the existing30-second total timeout. Only that fixture now has120 seconds;formal request gates unchanged.',denseRedV1='Terminal fixture failure on the unchanged legacy65536-node compatibility report limit. Original bytes retained;small report and every typed fact/full AST/IR compare separately.',denseRedV2='Actual old ordinary analysis matches all semantic outputs but cannot reuse the511 unchanged dense helpers.',focusedV1='Read-only scalar fix passes full semantic outputs and restores511 helpers;initial assertion overcounted one edited helper as unchanged. Original source/log preserved.',focusedV2='Corrected unchanged-helper set and explicit actual-matcher-refusal test mode;five focused native programs passed.'),protectedRuntime=seed['protected'],delivery=dict(rootReleaseChanged=False,guiRebuilt=False,guiInputAdmission=False,currentDeliveryIndexChanged=False),remaining=['Whole ordinary AST materialization and typed index directory production.','True local final IR and full module validation costs.','Larger workloads,all latency gates and global memory/long-run limits.','Current language frontend alignment,complete applicable LSP and matched SDK/IDE input acceptance.'],sourceSnapshots=files,reportArchives=reports)
out=R/'dev/release-materials/provenance/lsp-final-scalar-replay-20261103.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');(S/'manifest.json').write_text(json.dumps(dict(phase=report['phase'],scope='Exact source/controller/fault bytes and lossless original reports;not a runtime package.',files=files,reports=reports),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
verify_build(b,B);baseline()
for p,d in seed['protected'].items():assert sha(p)==d,p
print(json.dumps(dict(report=str(out),native=report['correctness'],hotP50=hot,failedGates={n:len(r['failedGates']) for n,r in performance.items()},snapshots=len(files),archives=len(reports)),ensure_ascii=False))
