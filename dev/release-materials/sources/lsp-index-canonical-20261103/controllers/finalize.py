from pathlib import Path
import json,hashlib,gzip,re,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text(encoding='utf8'))
seed=read(O/'source-base.json');A=Path(seed['baseline']);normal=read(B/'build-result.json');verify_build(normal,B)
base=read(A/'build-result.json');aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
def baseline():
 for p,d in base['inputs'].items():assert sha(aliases.get(p,p))==d,p
 for n,d in base['outputs'].items():assert sha(A/n)==d,n
baseline();validation=read(O/'validation-v1/result.json');assert validation['status']=='passed'
assert (R/'release/tmp').exists()==validation['rootTmpInitiallyPresent']
canonical=read(O/'canonical-faults-v1/result.json');assert canonical['status']=='passed'
geometry=read(O/'geometry-faults-v1/result.json');assert geometry['status']=='passed'
perf=read(O/'performance-v1/result.json');assert perf['status'] in ('completed','performance-not-admitted')
for p,d in seed['protected'].items():assert sha(p)==d,p
for p,d in validation['protected'].items():assert sha(p)==d,p
rules={p:d for p,d in normal['inputs'].items() if p.startswith(normal['frontendRoot']+'\\')}
for p,d in rules.items():assert base['inputs'][p]==d,p
header=Path(normal['privateAstConsumers'][0]['header']);assert sha(header)==sha(Path(base['privateAstConsumers'][0]['header']))
performance={};hot=[]
for name in ['aa','ab']:
 r=read(O/'performance-v1'/name/'result.json');assert all(p['exit']==0 and not p['cleanupErrors'] for p in r['processes'])
 performance[name]=dict(status=r['status'],queries=len(r['queries']),processes=len(r['processes']),gates=len(r['gates']),failedGates=[g for g in r['gates'] if not g['passed']],summary=r['summary'],minimumCommitGiB=min(x['availableCommit'] for x in r['resources'])/2**30,minimumPhysicalGiB=min(x['availablePhysical'] for x in r['resources'])/2**30,allProcessesExitZero=True,cleanupErrors=0)
 if name=='ab':
  performance[name]['observedProcessMemory']={variant:{k:max(q['memory'][k] for q in r['queries'] if q['variant']==variant) for k in r['queries'][0]['memory']} for variant in ['baseline','candidate']}
  for case,x in r['summary'].items():
   before=x['hot']['baseline']['ms']['p50'];after=x['hot']['candidate']['ms']['p50'];hot.append(dict(case=case,baselineMs=before,candidateMs=after,changePercent=100*(after/before-1)))
  observed={}
  for case in r['summary']:
   rows=[q['result']['analysis'].get('lastBuildIndexCanonical') for q in r['queries'] if q['case']==case and q['variant']=='candidate' and q['phase']=='hot']
   assert rows and all(x is not None for x in rows),(case,'missing actual wire statistics')
   observed[case]={k:dict(minimum=min(x[k] for x in rows),maximum=max(x[k] for x in rows)) for k in rows[0]}
  performance[name]['wireCanonicalWork']=observed
S=R/'dev/release-materials/sources/lsp-index-canonical-20261103';S.mkdir(parents=True,exist_ok=False);(S/'.gitattributes').write_text('* -text\n')
files=[];archives=[]
def snapshot(p,relative):
 p=Path(p);target=S/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(p.read_bytes());assert sha(p)==sha(target)
 files.append(dict(recordedPath=str(p),archivePath=target.relative_to(R).as_posix(),sha256=sha(p),bytes=p.stat().st_size))
owned=['ide/src/analysis/index.h','ide/src/analysis/check.h','ide/src/analysis/check.c','ide/src/analysis/frontend_index.h','ide/src/analysis/frontend_index.inc','ide/src/analysis/frontend_index_budget.inc','ide/src/analysis/frontend_index_canonical.inc','ide/src/analysis/frontend_function_index.inc','ide/src/analysis/frontend_sem.h','ide/src/analysis/frontend_sem.inc','ide/src/analysis/frontend_sem_ir.inc','ide/src/services/language_snapshot.c','ide/tests/analysis_index_canonical_test.c','ide/tests/analysis_index_canonical_guards_test.c','ide/tests/analysis_function_index_asset_test.c','ide/tools/native_index_overlay.py','ide/tools/build_lsp_core.py','ide/tools/check_index_canonical.py']
for p in owned:snapshot(R/p,Path('after')/p)
for row in seed['archivedInputs']:snapshot(Path(row['archivedPath']),Path('before')/Path(row['originalPath']).relative_to(R))
for p in O.glob('*.py'):snapshot(p,Path('controllers')/p.name)
for p in O.glob('*-test.c'):snapshot(p,Path('prototypes')/p.name)
for p in (O/'pre-proof-fix-inputs').rglob('*'):
 if p.is_file():snapshot(p,Path('pre-proof-fix')/p.relative_to(O/'pre-proof-fix-inputs'))
for folder in ['initial-probe','canonical-probe-v1','missing-prior-proof-v1','live-proof-probe-v1','extended-test-v1']:
 for p in (O/folder).glob('*.c'):snapshot(p,Path('fixtures')/folder/p.name)
for p in (O/'canonical-faults-v1').rglob('*'):
 if p.is_file() and p.suffix in ('.c','.h','.inc'):snapshot(p,Path('fault-sources')/'canonical-faults-v1'/p.relative_to(O/'canonical-faults-v1'))
paths={O/'source-base.json',O/'pre-proof-fix-inputs.json'}
if (O/'failed-full-v1-closure.json').exists():paths.add(O/'failed-full-v1-closure.json')
for p in O.rglob('*.json'):
 if p.name in ('result.json','plan.json','build-result.json','index-overlay.json') and not any(s in ('baseline-inputs','pre-proof-fix-inputs') for s in p.relative_to(O).parts):paths.add(p)
for folder in ['full-v1','full-v2','focused-v2','focused-v3','focused-v4','focused-v5','guards-v1','missing-prior-proof-v1','live-proof-probe-v1','extended-test-v1','extended-test-v2','initial-probe','canonical-probe-v1']:
 for p in (O/folder).glob('*.log'):
  if p.stat().st_size and ('run' in p.name or folder in ['full-v1','missing-prior-proof-v1','live-proof-probe-v1'] or p.name=='analysis_index_storage_test.log'):paths.add(p)
for p in sorted(paths):
 raw=p.read_bytes();target=S/'reports'/(str(p.relative_to(O))+'.gz');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(target.read_bytes())==raw
 archives.append(dict(recordedPath=str(p),originalSha256=sha(p),originalBytes=len(raw),archivePath=target.relative_to(R).as_posix(),archiveSha256=sha(target),archiveBytes=target.stat().st_size))
checks=int(re.search(r'passed:(\d+) checks',(B/'analysis_index_canonical_test-run.log').read_text()).group(1))
guards=int(re.search(r'passed:(\d+) checks',(B/'analysis_index_canonical_guards_test-run.log').read_text()).group(1))
wire=read(O/'wire-v1/result.json');omissions=read(O/'omissions-v1/result.json')
report=dict(phase='M7a2b115',date='2026-11-03',status='correctness-passed-comparison-gates-passed' if perf['accepted'] else 'correctness-passed-performance-not-admitted',accepted=False,sourceBase=seed['head'],scope='Independent canonical string assets authorized by live original semantic/final receipts;not full incremental index/IR,GUI or release admission.',implementation=dict(defaultCanonicalRetainBytes=8*1024*1024,nativeIndexBudgetBytes=32*1024*1024,originalCollectorRowsAndRulesUnchanged=True,sourceAndSymbolAndTypeFactsAlwaysCurrent=True,requiresLiveInputUnitFinalEnvironmentAndOutputReceipt=True,pairedPreviousMemoRequired=True,noAstSourceMemoIndexOrResultRetainedByCanonicalOwner=True,noHistoryChains=True,capacityAndFunctionAssetAccounting=True,optionalFailureUsesOriginalFormatter=True,partialShapeMismatchRetriesOriginalCollector=True,overhead='Cold sealing pass and XRT per-function scalar/string vectors;hot still visits every native fact and validates row kind/type presence.'),build=dict(path=str(B),reportSha256=sha(B/'build-result.json'),status=normal['status'],translationUnits=sum('-c' in c for c in normal['commands']),nativePrograms=len(normal['runs']),privateAstConsumers=len(normal['privateAstConsumers']),inputs=len(normal['inputs']),outputs=len(normal['outputs']),compilerSha256=normal['compilerSha256'],headerSha256=sha(header),unchangedFrontendRuleInputs=rules),baseline=dict(path=str(A),reportSha256=sha(A/'build-result.json'),status=base['status'],inputs=len(base['inputs']),outputs=len(base['outputs']),archivedChangedInputs=seed['archivedInputs']),correctness=dict(canonicalAssertions=checks,guardChecks=guards,completeCurrentTypedIndexAllFieldsAndNativeAstExtendedFieldsReportAndIrParity=True,oldAstRetiresWhileCurrentCanonicalValuesRemain=True,functionAssetOutlivesCheckAndSession=True,maximumFunctions=512,shiftedHotCanonicalCalls=18,shiftedColdCanonicalCalls=5650,shiftedHotValuesReused=5632,canonicalCases=len(canonical['cases']),canonicalCommands=len(canonical['commands']),geometryCases=len(geometry['cases']),nativeOmissionCases=len(omissions['cases']),protocolSuites=56,validationCommands=len(validation['commands']),wireSummary={k:wire[k] if k=='status' else len(wire[k]) for k in ['status','processes','checks','navigation','completions']}),performance=performance,hotP50=hot,history=dict(fullV1='Terminal compile failure:the new embedded native receipt test lacked the existing native warning profile;zero native tests ran.',focusedV2='Native borrowed-root suite passed,then controller requested a nonexistent fixture name;terminal StopIteration retained.',focusedV3='Two native suites passed;embedded collector test used the old generated collector path and failed strict unused-helper checking. Fresh generated include selected in focused-v4.',missingPriorProof='Actual private constructor accepted an old index without a prior memo by treating it as capture-only. Failing real call preserved;balanced-pair and explicit replay checks now reject it.',realStaleType='Actual C variant omitting old receipt proof produced stale canonical types after a transparent alias changed int to float;the current typed-index oracle rejected it.',invalidFixture='An initial test used double,which is not a builtin xlang type;changed to the native float spelling. Original source/results preserved.'),protectedRuntime=seed['protected'],delivery=dict(rootReleaseChanged=False,guiRebuilt=False,guiInputAdmission=False,currentDeliveryIndexChanged=False),remaining=['Whole ordinary AST materialization after source displacement.','Index directory/source/selection/entity production still eager;only canonical formatting reused.','Local final IR production,larger real-world workloads and all latency gates.','Current language frontend alignment,complete applicable LSP/IDE integration and matched runtime acceptance.'],sourceSnapshots=files,reportArchives=archives)
out=R/'dev/release-materials/provenance/lsp-index-canonical-20261103.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');(S/'manifest.json').write_text(json.dumps(dict(phase=report['phase'],scope='Exact source/controller bytes and lossless original reports;not a runtime package.',files=files,reports=archives),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
verify_build(normal,B);baseline()
for p,d in seed['protected'].items():assert sha(p)==d,p
print(json.dumps(dict(report=str(out),assertions=checks,guards=guards,hotP50=hot,failedGates={n:len(r['failedGates']) for n,r in performance.items()},snapshots=len(files),archives=len(archives)),ensure_ascii=False))
