from pathlib import Path
import json,gzip,hashlib,sys,subprocess,datetime
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2';A=O/'baseline-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text(encoding='utf8'))
b=read(B/'build-result.json');verify_build(b,B);v=read(O/'validation-v1/result.json');perf=read(O/'performance-v1/result.json');assert v['status']=='passed' and perf['status']=='performance-not-admitted'
runtime=read(O/'source-base.json')['protectedRuntime']
for p,d in {**runtime,**v['protected']}.items():assert sha(p)==d,p
base=read(A/'build-result.json');closure=read(O/'baseline-closure.json');aliases={x['recordedPath']:x['archivedPath'] for x in closure['archivedChangedInputs']}
for p,d in base['inputs'].items():assert sha(aliases.get(p,p))==d,p
for n,d in base['outputs'].items():assert sha(A/n)==d,n
S=R/'dev/release-materials/sources/lsp-native-prefix-20261103';S.mkdir(exist_ok=True);(S/'.gitattributes').write_text('* -text\n',encoding='ascii')
files=[]
def copy(src,dst):
 src=Path(src);dst=S/dst;dst.parent.mkdir(parents=True,exist_ok=True);data=src.read_bytes();dst.write_bytes(data);assert dst.read_bytes()==data
 files.append(dict(originalPath=str(src),storedPath=str(dst.relative_to(R)),sha256=sha(src),bytes=len(data)))
for rel in ['ide/src/analysis/ast_size.c','ide/src/analysis/ast_size.h','ide/src/analysis/typed_module.c','ide/src/analysis/typed_module_internal.h','ide/src/analysis/frontend_sem_native.inc','ide/tests/analysis_native_borrow_test.c','ide/tools/check_lsp_memo_types.py']:
 copy(R/rel,'current/'+rel)
for f in O.glob('*.py'):copy(f,'controllers/'+f.name)
for f in (O/'large-oracle-diagnostic-v1').glob('*.c'):copy(f,'large-oracle-diagnostic/'+f.name)
for row in closure['archivedChangedInputs']:copy(row['archivedPath'],'baseline-inputs/'+Path(row['recordedPath']).relative_to(R).as_posix())
failed=read(O/'full-v1/build-result.json');oldaliases={str(R/'ide/src/analysis/frontend_sem_native.inc'):O/'full-v1-sources/frontend_sem_native.inc',str(R/'ide/tests/analysis_native_borrow_test.c'):O/'large-oracle-diagnostic-v1/analysis_native_borrow_test.c'}
for path,d in failed['inputs'].items():assert sha(oldaliases.get(path,path))==d,path
for path,archived in oldaliases.items():copy(archived,'full-v1-inputs/'+Path(path).relative_to(R).as_posix())
archives=[]
def archive(src):
 src=Path(src);raw=src.read_bytes();dst=S/'reports'/(str(src.relative_to(O)).replace('\\','__').replace('/','__')+'.gz');dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(dst.read_bytes())==raw
 row=dict(originalPath=str(src),storedPath=str(dst.relative_to(R)),originalSha256=sha(src),originalBytes=len(raw),storedSha256=sha(dst),storedBytes=dst.stat().st_size);archives.append(row);return row
paths=['frontend-recovery.json','source-base.json','baseline-closure.json','baseline-v2/build-result.json','full-v1/build-result.json','full-v1/analysis_native_borrow_test-run.log','full-v2/build-result.json','full-v2/analysis_native_borrow_test-run.log','native-diagnostic-v1/result.json','native-metrics-v1/result.json','candidate-fast-v1/result.json','partial-metrics-v1/result.json','large-oracle-diagnostic-v1/run.log','focused-v2/result.json','focused-v2/native-test.log','validation-v1/result.json','omissions-v1/result.json','wire-v1/result.json','performance-v1/result.json','performance-v1/aa/result.json','performance-v1/aa/plan.json','performance-v1/ab/result.json','performance-v1/ab/plan.json','declaration-cause.json']
for rel in paths:archive(O/rel)
for rel in ['decline-metrics-v1','decline-metrics-v2','decline-metrics-v3']:
 archive(O/rel/'result.json')
 for f in (O/rel).rglob('*candidate-stderr.log'):archive(f)
for rel in ['native-metrics-v1']:
 for f in (O/rel).rglob('*candidate-stderr.log'):archive(f)
rule=[]
for rel in ['frontend/xl6_sem.c','frontend/xl6_sem_c_owned.effects.c']:
 assert sha(A/rel)==sha(B/rel),rel
 rule.append(dict(path=rel,sha256=sha(B/rel),identicalToBaseline=True))
assert sha(Path(base['privateAstConsumers'][0]['header']))==sha(Path(b['privateAstConsumers'][0]['header']))
performance={};hot=[]
for n in ['aa','ab']:
 d=read(O/'performance-v1'/n/'result.json');assert all(r['exit']==0 and not r['cleanupErrors'] for r in d['processes'])
 performance[n]=dict(status=d['status'],queries=len(d['queries']),processes=len(d['processes']),gates=len(d['gates']),failedGates=[r for r in d['gates'] if not r['passed']],summary=d['summary'],minimumCommitGiB=min(r['availableCommit'] for r in d['resources'])/2**30,minimumPhysicalGiB=min(r['availablePhysical'] for r in d['resources'])/2**30,allProcessesExitZero=True,cleanupErrors=0)
 if n=='ab':
  for case,x in d['summary'].items():
   left=x['hot']['baseline']['ms']['p50'];right=x['hot']['candidate']['ms']['p50'];hot.append(dict(case=case,baselineMs=left,candidateMs=right,changePercent=100*(right/left-1)))
  performance[n]['observedProcessMemory']={variant:{k:max(q['memory'][k] for q in d['queries'] if q['variant']==variant) for k in d['queries'][0]['memory']} for variant in ['baseline','candidate']}
wire=read(O/'wire-v1/result.json')
report=dict(phase='M7a2b113',date='2026-11-03',status='correctness-passed-performance-not-admitted',accepted=False,scope='Bounded native function prefix receipts and reuse;not full LSP,GUI,or release delivery.',sourceBase='a82489753',implementation=dict(defaultNativeRetainBytes=32*1024*1024,fullPoolBackingCharged=True,forestMetadataChargedInFull=True,ledgerBytes=128,prefixFailureCountersNotCommitted=True,outsidePrefixKnownBytes=0,semanticAdmissionUnchanged=True,skipReadonlyScanWithoutReceipt=True),build=dict(path=str(B),reportSha256=sha(B/'build-result.json'),status=b['status'],translationUnits=230,nativePrograms=len(b['runs']),privateAstConsumers=len(b['privateAstConsumers']),inputs=len(b['inputs']),outputs=len(b['outputs']),compilerSha256=b['compilerSha256'],headerSha256=sha(Path(b['privateAstConsumers'][0]['header'])),ruleSourceIdentity=rule),correctness=dict(nativePrefixChecks=1216291,largeFunctions=512,largeGenerations=3,retainedAndReused=85,copiedNodesCold=97820,copiedNodesHot=82398,knownRetainBytes=33487635,fullAstExtendedFieldsNativeFactsAndIrParity=True,partialBudgetMalformedCancellationAndRetirement=True,omissionCases=9,omissionCommands=29,validationCommands=len(v['commands']),protocolSuites=56,wireSummary={k:wire[k] if k=='status' else len(wire[k]) for k in ['status','processes','checks','navigation','completions']}),baseline=dict(path=str(A),reportSha256=sha(A/'build-result.json'),status=base['status'],testsBuilt=base['testsBuilt'],scope='Fresh build-only comparison image at current host SHA implementation;not a full native-test build.',inputCount=len(base['inputs']),outputCount=len(base['outputs']),archivedChangedInputs=closure['archivedChangedInputs']),performance=performance,hotP50=hot,history=dict(fullV1=dict(status='failed',translationUnits=230,passedNativePrograms=len(failed['runs']),cause='Both large compatibility JSON index exports hit the original32MiB byte limit;native oracle now compares every fact,all AST fields,report and IR without raising quota.',inputClosureWithArchivedBytesPassed=True),interruptedBaselineV1='Process handle missing and no final report;not counted as a completed build. Fresh baseline-v2 built afterward.',diagnostics='Diagnostic timings are not formal admission. The first probe patched semantic.o rather than the xl6_sem.o owner of BindFunctions;its size/result counters remain recorded,missing bind output is not evidence.'),protectedRuntime=runtime,delivery=dict(rootReleaseChanged=False,guiRebuilt=False,guiInputAdmission=False,currentDeliveryIndexChanged=False),remaining=['No performance admission:AA3 and AB6 original gates failed,all retained without reruns.','Current foreign type origin/header coordinate proof across multi-site edits;32 declaration retries remain.','Pool granularity and retention beyond a prefix.','Ordinary index reuse,local final IR production,methods/nested binding and full applicable LSP/IDE/performance/scale acceptance.'],reportArchives=archives,sourceSnapshots=files)
P=R/'dev/release-materials/provenance/lsp-native-prefix-20261103.json';P.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
(S/'manifest.json').write_text(json.dumps(dict(phase=report['phase'],scope='Raw source/controller snapshots and lossless gzip copies of reports;original files and recorded paths retained. Not a runtime package.',files=files,reports=archives),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
verify_build(b,B)
for path,digest in {**runtime,**v['protected']}.items():assert sha(path)==digest,path
print('Stage evidence saved',P,'archives',len(archives),'snapshots',len(files));print('wire keys',list(wire))
