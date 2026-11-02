from pathlib import Path
import gc,hashlib,json,shutil,sys
ROOT=Path('D:/GIT/x-lang')
A=ROOT/'ide/build/Release/lsp-native-borrow-20261102'
E=Path(__file__).resolve().parent
B=E/'full-v4'
sys.path.insert(0,str(ROOT/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build,sha

def read(path):return json.loads(Path(path).read_text('utf8'))
def receipt(path):
 p=Path(path);return dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size)
def protect(record):
 for path,digest in record.get('protected',{}).items():assert sha(path)==digest,path
def logs(record):
 for row in record.get('commands',[]):
  if 'log' in row and 'sha256' in row:assert sha(row['log'])==row['sha256'],row['log']

normal=read(B/'build-result.json');verify_build(normal,B)
validation=read(E/'validation-v1/result.json');assert validation['status']=='passed' and validation['suites']==56
omissions=read(E/'omissions-v1/result.json');assert omissions['status']=='passed' and len(omissions['cases'])==9
wire=read(E/'wire-v1/result.json');assert wire['status']=='passed'
controller=read(E/'analysis-v1/result.json');assert controller['status']=='complete' and not controller['accepted']
for record in [validation,omissions,wire,controller]:protect(record);logs(record)
for process in wire['processes']:
 if 'exit' in process:assert process['exit']==0
 for kind in ['stderr','transcript']:
  if kind+'Sha256' in process:assert sha(process[kind])==process[kind+'Sha256']

performance=[]
for job in controller['jobs']:
 path=E/'analysis-v1'/job['name']/'result.json';raw=read(path)
 assert raw['status'] in ['calibration-failed','gates-failed'] and not raw['accepted']
 assert sha(raw['plan'])==raw['planSha256']
 for row in raw['resources']:assert min(row['availablePhysical'],row['availableCommit'])>=row['requiredGiB']*1024**3
 for process in raw['processes']:
  assert process['exit']==0 and not process['cleanupErrors']
  for kind in ['stderr','transcript']:assert sha(process[kind])==process[kind+'Sha256'],process[kind]
 failed=[gate for gate in raw['gates'] if not gate['passed']]
 performance.append(dict(name=job['name'],**receipt(path),status=raw['status'],queries=len(raw['queries']),processes=len(raw['processes']),gates=len(raw['gates']),failedGates=failed,minimumObservedCommitGiB=min(row['availableCommit'] for row in raw['resources'])/1024**3,minimumObservedPhysicalGiB=min(row['availablePhysical'] for row in raw['resources'])/1024**3,allProcessesExitedZero=True,cleanupErrors=0,allProcessArtifactHashesVerified=True,summary=raw['summary']))
 del raw;gc.collect()
assert [len(job['failedGates']) for job in performance]==[1,6]

freeze=read(A/'full-source-freeze-v3.json')
for rel,digest in freeze.items():assert sha(ROOT/rel)==digest,rel
capture=read(A/'capture.json')
for rel,row in capture['files'].items():assert sha(row['path'])==row['sha256'],rel
for folder in (A/'history').iterdir():
 manifest=folder/'manifest.json'
 if manifest.exists():
  for path,row in read(manifest).items():
   if isinstance(row,dict) and 'sha256' in row:
    source=row.get('path',row.get('archivedPath'))
    if source:assert sha(source)==row['sha256'],source

history=E/'evidence-capsule';history.mkdir(exist_ok=False);entries=[]
def keep(source,target,role,recorded=None):
 source=Path(source);target=history/target;target.parent.mkdir(parents=True,exist_ok=True)
 target.write_bytes(source.read_bytes());assert sha(target)==sha(source)
 entries.append(dict(role=role,recordedPath=str(recorded or source),archivedPath=target.relative_to(history).as_posix(),sha256=sha(target),bytes=target.stat().st_size))
for folder,base in [(A/'previous-ide-inputs',Path('previous-ide-inputs')),(A/'history',Path('diagnostic-history')),(E/'history',Path('e-controller-history'))]:
 for p in sorted(folder.rglob('*')):
  if p.is_file():keep(p,base/p.relative_to(folder),'raw captured historical source/controller; original report paths unchanged')
for p in sorted(A.iterdir()):
 if p.is_file() and (p.suffix=='.py' or p.name in ['capture.json','full-source-freeze.json','full-source-freeze-v2.json','full-source-freeze-v3.json','native-rule-proof-v1.json']):keep(p,Path('d-controllers')/p.name,'original D controller or fixed capture')
for p in sorted(E.iterdir()):
 if p.is_file() and (p.suffix=='.py' or p.name in ['capture.json','full-source-freeze-v3.json','native-rule-proof-v1.json']):keep(p,Path('e-controllers')/p.name,'executed E controller or source/rule capture')
for rel in freeze:keep(ROOT/rel,Path('final-ide-inputs')/rel,'exact final frozen build input')
keep(ROOT/'ide/tools/check_native_borrow.py',Path('final-ide-inputs/ide/tools/check_native_borrow.py'),'executed omission/allocation controller')
manifest=dict(schema=1,phase='M7a2b112',scope='Raw source history and fixed controllers,not a runtime or complete product closure. Existing failed reports retain bytes and recorded paths.',entries=entries)
(history/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8',newline='\n')
(history/'.gitattributes').write_bytes(b'* -text -whitespace\n')
(history/'README.md').write_text('# M112 历史输入与控制器\n\n保存 M111 的 16 项原始输入、本轮已捕获的各阶段源码、20 项最终冻结输入和实际执行控制器。原始失败、构建、协议与计时报告保留其 D/E 盘记录路径及字节；本目录不是完整产品闭包或运行资源。\n\nmanifest.json 记录每份原字节的 SHA-256。diagnostic-history 的原清单保留原路径，外层清单指向本目录的对应副本。首次 E 控制器的旧基线路径诊断保留；修正后规则核对通过，性能只执行固定一轮 A/A 和 A/B。\n',encoding='utf8',newline='\n')

attempts=[]
for stage in ['focused-v1','focused-v2','focused-v3','focused-v4','focused-v5','native-diagnostic-v1','native-diagnostic-v2','native-diagnostic-v3']:
 path=A/stage/'result.json';raw=read(path)
 attempts.append(dict(name=stage,**receipt(path),status=raw['status'],cases=raw.get('cases',[])))
for stage in ['full-v1','full-v2','full-v3']:
 path=A/stage/'build-result.json';raw=read(path)
 attempts.append(dict(name=stage,**receipt(path),status=raw['status'],translationUnits=sum('-c' in c for c in raw['commands']),nativeRuns=len(raw.get('runs',[])),error=raw.get('error')))
rule=read(E/'native-rule-proof-v1.json');assert rule['status']=='passed' and all(row['exactlyEqualToM111'] for row in rule['rows'])
oldHeader=ROOT/'ide/build/Release/lsp-composite-functions-20261102/full-v1/frontend/src/ast/xl6_ast.h'
header=B/'frontend/src/ast/xl6_ast.h';assert sha(header)==sha(oldHeader)
record=dict(phase='M7a2b112',date='2026-11-02',status='correctness-validated-performance-gates-failed',accepted=False,
 scope='Ordinary checks borrow independently owned verified native top-level functions. Full original C-owned validation, index collection and final IR construction/verification remain. Not full LSP/GUI/release admission.',
 implementation=dict(admission='Independent immutable raw unit plus exact current source range,line/column,symbol/type intervals,positioned type vector,complete final environment and original region interval. All candidates verified before allocation/callback-free installation.',ownership='M111 forest references and shared XRT locks; detached current headers retained until semantic context/type registry retirement; no old check/module/report retention.',writeSafety='Conservative certificate using original C-owned name recognition excludes possible original-validator writes. Full original validator before and after composition; final type/plan hooks skip verified immutable functions.',quota='Whole known pool backing snapshot plus actual capacity/string/source costs,not logical subtree size or RSS. Each forest charged once;default32MiB;controls only shrink.',fallback='Discard entire private omitted attempt and retry once with original full source. Cancellation not retried. Materialization statistics include both attempts.',remainingEager='Full index and final IR;methods,nested declarations,shifted functions or uncertain proofs use original path.',testObservation=dict(checks=805,unchangedFunctionsReused=60,successiveEdits=3,copiedNodesBefore=4578,copiedNodesAfter=558)),
 build=dict(**receipt(B/'build-result.json'),translationUnits=sum('-c' in c for c in normal['commands']),nativeFixtures=len(normal['runs']),privateAstConsumers=len(normal['privateAstConsumers']),inputs=len(normal['inputs']),outputs=len(normal['outputs']),compiler=normal['compiler'],compilerSha256=normal['compilerSha256'],serverSha256=sha(B/'xlang-ls.exe'),privateAstHeaderSha256=sha(header),privateAstLayoutUnchangedFromM111=True,fixedInputCapture=receipt(A/'full-source-freeze-v3.json')),
 correctness=dict(nativeRuleBodyProof=dict(**receipt(E/'native-rule-proof-v1.json'),record=rule),omissions=dict(**receipt(E/'omissions-v1/result.json'),status=omissions['status'],cases=omissions['cases'],commands=len(omissions['commands'])),wire=dict(**receipt(E/'wire-v1/result.json'),**{k:len(wire[k]) for k in ['processes','checks','navigation','completions']}),validation=dict(**receipt(E/'validation-v1/result.json'),status=validation['status'],suites=validation['suites'],commands=len(validation['commands'])),baselineClosure=validation['baselineClosure']),
 performance=dict(controller=dict(**receipt(E/'analysis-v1/result.json'),record=controller),jobs=performance,admitted=False,conclusion='32-function literal hot medians improve about29-31percent,but declaration cases regress about19percent and512-function hot medians regress about2-3percent. A/A1of64 andA/B6of64 gates fail. No overall latency admission;all samples retained,no unchanged reruns.'),
 diagnosticReports=attempts,history=dict(path='dev/release-materials/sources/lsp-native-borrow-history-20261102/manifest.json',sha256=sha(history/'manifest.json'),archivedFiles=len(entries)),
 protectedRuntime={p:d for p,d in validation['protected'].items() if Path(p).is_relative_to(ROOT/'release')},
 delivery=dict(rootIdeOrServerInstalled=False,activeDeliveryIndexUpdated=False,reasonForPrivateEnvironment='Matching M111 comparisons and real faults protect the working release;D disk exhausted duringfull-v3 linking,user cannot free requested space,E used for the full rebuild and acceptance.',storageAction='Automatic approval review rejected deleting original build directory to create a path mapping. No part executed; originals preserved,no junction. Small source/evidence commit only after free-space check.'),
 remaining=['Actual current bindings for shifted/symbol/type/region changes and methods/nested declarations','Index asset composition and local final IR production;reduce declaration fallback/cold costs','Complete applicable LSP/IDE,matched SDK/engine/GUI typing and large-scale end-to-end performance admission'],inputClosure=normal['inputs'],outputClosure=normal['outputs'])
out=E/'lsp-native-borrow-20261102.json'
with out.open('x',encoding='utf8',newline='\n') as stream:stream.write(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(path=str(out),bytes=out.stat().st_size,sha256=sha(out),sourceFiles=len(entries),capsuleBytes=sum(p.stat().st_size for p in history.rglob('*') if p.is_file()),validationCommands=len(validation['commands']),wireCounts={k:len(wire[k]) for k in ['processes','checks','navigation','completions']})))
