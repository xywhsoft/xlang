from pathlib import Path
import json,hashlib,gzip,re,sys
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v2'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text(encoding='utf8'))
seed=read(O/'source-base.json');A=Path(seed['baseline']);b=read(B/'build-result.json');verify_build(b,B);base=read(A/'build-result.json')
aliases={r['originalPath']:r['archivedPath'] for r in seed['archivedInputs']}
for p,d in base['inputs'].items():assert sha(aliases.get(p,p))==d,p
for n,d in base['outputs'].items():assert sha(A/n)==d,n
v=read(O/'validation-v1/result.json');assert v['status']=='passed';assert (R/'release/tmp').exists()==v['rootTmpInitiallyPresent']
g=read(O/'geometry-faults-v1/result.json');assert g['status']=='passed'
moving=read(O/'moving-v2/result.json');assert moving['status']=='passed'
for path,digest in moving['protected'].items():assert sha(path)==digest,path
perf=read(O/'performance-v1/result.json');assert perf['status'] in ('completed','performance-not-admitted')
for p,d in seed['protected'].items():assert sha(p)==d,p
for p,d in v['protected'].items():assert sha(p)==d,p
rules={p:d for p,d in b['inputs'].items() if p.startswith(b['frontendRoot']+'\\')}
for p,d in rules.items():assert base['inputs'][p]==d,p
header=Path(b['privateAstConsumers'][0]['header']);assert sha(header)==sha(Path(base['privateAstConsumers'][0]['header']))
performance={};hot=[]
for n in ['aa','ab']:
 d=read(O/'performance-v1'/n/'result.json');assert all(p['exit']==0 and not p['cleanupErrors'] for p in d['processes'])
 performance[n]=dict(status=d['status'],queries=len(d['queries']),processes=len(d['processes']),gates=len(d['gates']),failedGates=[x for x in d['gates'] if not x['passed']],summary=d['summary'],minimumCommitGiB=min(x['availableCommit'] for x in d['resources'])/2**30,minimumPhysicalGiB=min(x['availablePhysical'] for x in d['resources'])/2**30,allProcessesExitZero=True,cleanupErrors=0)
 if n=='ab':
  for case,x in d['summary'].items():
   left=x['hot']['baseline']['ms']['p50'];right=x['hot']['candidate']['ms']['p50'];hot.append(dict(case=case,baselineMs=left,candidateMs=right,changePercent=100*(right/left-1)))
  performance[n]['observedProcessMemory']={variant:{k:max(q['memory'][k] for q in d['queries'] if q['variant']==variant) for k in d['queries'][0]['memory']} for variant in ['baseline','candidate']}
S=R/'dev/release-materials/sources/lsp-semantic-header-geometry-20261103';S.mkdir(parents=True,exist_ok=False);(S/'.gitattributes').write_text('* -text\n')
files=[];archives=[]
def snapshot(p,relative):
 p=Path(p);target=S/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(p.read_bytes());assert sha(target)==sha(p);files.append(dict(recordedPath=str(p),archivePath=str(target.relative_to(R)).replace('\\','/'),sha256=sha(p),bytes=p.stat().st_size))
for p in ['ide/src/analysis/check.c','ide/src/analysis/frontend_sem.h','ide/src/analysis/frontend_sem.inc','ide/src/analysis/frontend_sem_body.inc','ide/src/analysis/frontend_sem_proof.inc','ide/src/analysis/frontend_sem_geometry.inc','ide/tests/analysis_native_borrow_test.c','ide/tests/analysis_final_environment_test.c','ide/tools/check_semantic_geometry.py','ide/tools/check_semantic_geometry_moving.py']:
 snapshot(R/p,Path('after')/p)
for row in seed['archivedInputs']:snapshot(Path(row['archivedPath']),Path('before')/Path(row['originalPath']).relative_to(R))
for p in O.glob('*.py'):snapshot(p,Path('controllers')/p.name)
for p in (O/'prototype-v1-inputs').rglob('*'):
 if p.is_file():snapshot(p,Path('prototype')/p.relative_to(O/'prototype-v1-inputs'))
for folder in ['moving-v1','moving-v2','moving-diagnostic-v1']:snapshot(O/folder/'moving.c',Path('fixtures')/folder/'moving.c')
paths=set()
for name in ['source-base.json','prototype-v1-inputs.json','producer-audit.json','producer-audit-prototype-v1.json','failed-full-v1-closure.json']:paths.add(O/name)
for p in O.rglob('*.json'):
 if p.name in ('build-result.json','result.json','plan.json','resume.json','resume-v2.json') and not any(part in ('baseline-inputs','prototype-v1-inputs') for part in p.relative_to(O).parts):paths.add(p)
for folder in ['focused-v1','focused-v2','focused-v3','focused-v4','focused-v5','producer-probe-v1','producer-probe-v2','full-v1','full-v2','moving-v1','moving-v2','moving-diagnostic-v1']:
 for p in (O/folder).glob('*.log'):
  if p.stat().st_size and ('run' in p.name or folder in ('focused-v1','producer-probe-v1','producer-probe-v2') or (folder=='full-v1' and 'headers' in p.name)):paths.add(p)
for p in sorted(paths):
 raw=p.read_bytes();target=S/'reports'/(str(p.relative_to(O))+'.gz');target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(target.read_bytes())==raw
 archives.append(dict(recordedPath=str(p),originalSha256=sha(p),originalBytes=len(raw),archivePath=str(target.relative_to(R)).replace('\\','/'),archiveSha256=sha(target),archiveBytes=target.stat().st_size))
log=(B/'analysis_native_borrow_test-run.log').read_text();checks=int(re.search(r'passed:(\d+) checks',log).group(1));f=read(O/'focused-v5/result.json');assert f['status']=='passed'
wire=read(O/'wire-v1/result.json');omissions=read(O/'omissions-v1/result.json')
report=dict(phase='M7a2b114',date='2026-11-03',status='correctness-passed-performance-admitted-for-this-comparison' if perf['accepted'] else 'correctness-passed-performance-not-admitted',accepted=False,scope='Certified direct-call header geometry through existing original semantic context;full LSP,GUI and release acceptance remains incomplete.',sourceBase=seed['head'],implementation=dict(optionalRetainedGeometryBytes=8*1024*1024,maximumGeometryRows=65536,existingMemoBudgetBytes=64*1024*1024,existingTypeProofBudgetBytes=4096,usesOriginalContextWalk=True,edgeProducerCertificates=True,actualInternAliasAndLoopProducerTest=True,nativeAstAbiChanged=False,originalSemanticsChanged=False,wholeContextStillRequired=True,resourceFailureUsesOriginalPath=True,overhead='One optional immutable scalar/path asset,an origin ID per located proof edge,and sorted lookup on previously declined coordinates;all retained capacities charged.'),build=dict(path=str(B),reportSha256=sha(B/'build-result.json'),status=b['status'],translationUnits=sum('-c' in c for c in b['commands']),nativePrograms=len(b['runs']),privateAstConsumers=len(b['privateAstConsumers']),inputs=len(b['inputs']),outputs=len(b['outputs']),compilerSha256=b['compilerSha256'],headerSha256=sha(header),unchangedFrontendRuleInputs=rules),correctness=dict(nativeAssertions=checks,fullFreshAstExtendedFieldsIndexAndIrParity=True,normalHotNativeFunctions=61,normalColdCopiedNodes=4597,normalHotCopiedNodes=573,normalHotMappedEdges=3,normalHotRetries=0,movingFallback=dict(generations=moving['movingGenerations'],assertions=moving['nativeAssertions'],requiresNativeAbsoluteRangeUnchanged=True),geometryGuardAndResourceCases=len(g['cases']),geometryGuardAndResourceCommands=len(g['commands']),originalNativeOmissionCases=len(omissions['cases']),protocolSuites=56,validationCommands=len(v['commands']),wireSummary={k:wire[k] if k=='status' else len(wire[k]) for k in ['status','processes','checks','navigation','completions']}),baseline=dict(path=str(A),reportSha256=sha(A/'build-result.json'),status=base['status'],nativePrograms=len(base['runs']),inputs=len(base['inputs']),outputs=len(base['outputs']),archivedChangedInputs=seed['archivedInputs']),performance=performance,hotP50=hot,history=dict(fullV1='Strict warning rejected misleading indentation;no native tests ran. Preserved terminal failed build.',focusedV4='Producer-certified prototype failed its native geometry expectation;name symbol is a global overload group,not necessarily selected declaration. Raw running status is stale;command exit1 and terminal handle establish failure. Corrected parent-call proof passes focused-v5.',earlierPrototypes='Coordinate-only prototype was not admitted. Real for-target source/range and line/column hybrid plus interning exposed missing edge provenance;all earlier bytes/results preserved.',movingControllerV1=dict(recordedPath=str(R/'ide/tools/check_semantic_geometry_moving.py'),preservedPath=str(O/'moving-controller-v1.py'),sha256=sha(O/'moving-controller-v1.py')),movingV1='The supplemental test incorrectly expected61 unchanged absolute-coordinate native roots after source widths grew;fresh parity passed but alias expectation failed. Existing complete lexical-range admission correctly rejects shifted trees. moving-v2 verifies conservative no-alias full parity across0/100/10000;old controller and fixture bytes preserved.',privateProbeControllers='Missing include and unused-function compile failures preserved;only final resumed probe ran and established the overload-group IDs.'),protectedRuntime=seed['protected'],delivery=dict(rootReleaseChanged=False,guiRebuilt=False,guiInputAdmission=False,currentDeliveryIndexChanged=False),remaining=['Performance and tail-latency gates are retained without reruns or waivers.','Coordinate-aware incremental units:ordinary native borrowing currently requires unchanged absolute lexical/type positions;source-width shifts still materialize full trees. Local ordinary index/final IR production and larger/real-world workloads.','Broader provable producers:methods,nested binding,generics,async and standalone callable values;original full analysis remains available.','Current language frontend alignment,complete applicable LSP/IDE integration and matched runtime acceptance.'],sourceSnapshots=files,reportArchives=archives)
output=R/'dev/release-materials/provenance/lsp-semantic-header-geometry-20261103.json';output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
(S/'manifest.json').write_text(json.dumps(dict(phase=report['phase'],scope='Raw source/controller inputs and lossless original report archives;not a runtime package.',files=files,reports=archives),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
verify_build(b,B)
for p,d in seed['protected'].items():assert sha(p)==d,p
print(json.dumps(dict(report=str(output),nativeAssertions=checks,hotP50=hot,failedGates={n:len(x['failedGates']) for n,x in performance.items()},snapshots=len(files),archives=len(archives)),ensure_ascii=False))
