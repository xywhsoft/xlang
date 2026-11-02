from pathlib import Path
import json,sys
R=Path(__file__).resolve().parents[4];A=Path(__file__).resolve().parent;B=A/'full-v1'
sys.path.insert(0,str(R/'ide/tools'))
from build_lsp_lifecycle_profile import verify_build,sha

def receipt(p):
 p=Path(p);return dict(path=str(p),sha256=sha(p),bytes=p.stat().st_size)
normal=json.loads((B/'build-result.json').read_text());verify_build(normal,B)
validation=json.loads((A/'validation-v1/result.json').read_text());assert validation['status']=='passed' and validation['suites']==56
omissions=json.loads((A/'omissions-v1/result.json').read_text());assert omissions['status']=='passed' and len(omissions['cases'])==7
wire=json.loads((A/'wire-v1/result.json').read_text());assert wire['status']=='passed'
for record in [validation,omissions]:
 for path,digest in record['protected'].items():assert sha(path)==digest,path
for rel,digest in json.loads((A/'full-source-freeze.json').read_text()).items():assert sha(R/rel)==digest,rel
oldHeader=A.parent/'lsp-final-annotations-20261102/full-v1/frontend/src/ast/xl6_ast.h';header=B/'frontend/src/ast/xl6_ast.h';assert sha(header)==sha(oldHeader)
history=R/'dev/release-materials/sources/lsp-composite-functions-history-20261102';history.mkdir(parents=True,exist_ok=False);entries=[]
def keep(source,target,role,recorded=None):
 target=history/target;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(source.read_bytes());assert sha(target)==sha(source)
 entries.append(dict(role=role,recordedPath=str(recorded or source),archivedPath=target.relative_to(R).as_posix(),sha256=sha(target),bytes=target.stat().st_size))
for rel,row in json.loads((A/'capture.json').read_text())['files'].items():
 p=Path(row['path']);assert sha(p)==row['sha256'];keep(p,Path('previous-ide-inputs')/rel,'exact pre-edit matching M110 input',R/rel)
for p in sorted((A/'history').rglob('*')):
 if p.is_file():keep(p,Path('history')/p.relative_to(A/'history'),'captured actual intermediate diagnostic input')
for name in ['capture.json','full-source-freeze.json','run-focused-v1.py','run-focused-v2.py','run-focused-v3.py','run-focused-v4.py','run-validation-v1.py','assemble-evidence.py']:
 keep(A/name,Path(name),'fixed controller or input capture')
(history/'.gitattributes').write_bytes(b'* -text -whitespace\n')
(history/'README.md').write_text('# M111 historical inputs and controllers\n\nExact pre-edit M110 bytes and actually captured failed diagnostic inputs. Original report bytes and paths are unchanged. These materials are not runtime resources or complete product build closures.\n',encoding='utf8',newline='\n')
manifest=dict(schema=1,phase='M7a2b111',entries=entries)
(history/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8',newline='\n')
# Original diagnostics stay immutable. Changed input references must resolve to
# either current exact bytes or a captured raw version;no arbitrary pin bypass.
diagnostics=[]
for name in ['focused-v1','focused-v2','focused-v3','focused-v4']:
 raw=json.loads((A/name/'result.json').read_text());aliases=[]
 for path,digest in raw['protected'].items():
  if sha(path)!=digest:
   found=[row for row in entries if row['sha256']==digest];assert found,(name,path,digest)
   aliases.append(dict(recordedPath=path,sha256=digest,archivedPath=found[0]['archivedPath']))
 diagnostics.append(dict(**receipt(A/name/'result.json'),status=raw['status'],commands=len(raw['commands']),exactArchivedChangedInputs=aliases))
assert [d['status'] for d in diagnostics]==['failed','failed','failed','passed']
record=dict(phase='M7a2b111',date='2026-11-02',status='correctness-validated-preparatory-interface',accepted=False,
 scope='Compose independent finalized native function owners into a new typed module,with exact per-owner known-metadata quota. Ordinary AST borrowing is not enabled;not complete LSP/performance/GUI/release admission.',
 implementation=dict(sharedOriginalRules=True,noBorrowedAstClone=True,retainsOldModuleOrCheck=False,construction='Validate exact slot/root matching and complete mandatory allocations before taking root;independently retain borrowed functions;only fresh functions enter new forest',retirement='Detach borrowed roots before fresh teardown;release original function owners without nested forest locks',ir='IR results independently retain the complete composite input',metadataQuota='Standalone units fully charge known native/index metadata;sets deduplicate each retained forest and each current index catalog exactly once using a demand-only XRT ledger',normalCost='Larger opaque module record and empty borrowed-slot lookup;ordinary checking never constructs the ledger or borrows old functions yet',explicitCosts='Mandatory composite pointer vector,atomic references,temporary slot detachment;two temporary XRT lookup maps during explicit set capture',metadataIdentity='Ownership-only pointer plus group-kind tag;never input/semantic freshness',bounds=dict(moduleSlots=65536,singleFunctionBytes=67108864,functionSetBytes=268435456),memoryScope='Known retained AST/string/container/metadata quota only;shared native pool backing,allocator and transient maps remain additional'),
 build=dict(**receipt(B/'build-result.json'),translationUnits=sum('-c' in c for c in normal['commands']),nativeFixtures=len(normal['runs']),privateAstConsumers=len(normal['privateAstConsumers']),inputs=len(normal['inputs']),outputs=len(normal['outputs']),compiler=normal['compiler'],compilerSha256=normal['compilerSha256'],serverSha256=sha(B/'xlang-ls.exe'),privateAstHeaderSha256=sha(header),privateAstLayoutUnchangedFromM110=True,sourceFreeze=receipt(A/'full-source-freeze.json')),
 correctness=dict(nativeRuns=[run for run in normal['runs'] if run['name'] in {'analysis_composite_function_test','analysis_typed_function_test','analysis_function_unit_test','analysis_function_unit_fault_test','analysis_function_index_asset_test','analysis_typed_module_test','analysis_typed_module_fault_test'}],omissions=dict(**receipt(A/'omissions-v1/result.json'),cases=omissions['cases'],commands=len(omissions['commands'])),wire=dict(**receipt(A/'wire-v1/result.json'),**{k:len(wire[k]) for k in ['processes','checks','navigation','completions']}),validation=dict(**receipt(A/'validation-v1/result.json'),suites=validation['suites'],commands=len(validation['commands'])),baselineClosure=validation['baselineClosure']),
 diagnostics=diagnostics,history=dict(**receipt(history/'manifest.json'),archivedFiles=len(entries)),
 performance=dict(measured=False,admitted=False,reason='Preparatory ownership/explicit-capture interface,ordinary omission not yet enabled;do not rerun unchanged M110 analysis to select a passing result or relabel its one failed gate.'),
 protectedRuntime={p:d for p,d in validation['protected'].items() if Path(p).is_relative_to(R/'release')},
 delivery=dict(rootIdeOrServerInstalled=False,activeDeliveryIndexUpdated=False,reasonForPrivateEnvironment='Matching full cohort,actual ownership/ledger omission injection,and protection of the working release'),
 remaining=['Bind current raw/semantic/final proofs to independent native owners with bounded retention','Enable ordinary AST omission only after exact current input/source/symbol/type/region proof;discard and eagerly retry failed private attempts','Final IR local production/composition','Complete end-to-end latency,scale,memory,matched engine/SDK/IDE,C/xlang GUI and remaining applicable LSP'],inputClosure=normal['inputs'],outputClosure=normal['outputs'])
target=R/'dev/release-materials/provenance/lsp-composite-functions-20261102.json'
with target.open('x',encoding='utf8',newline='\n') as stream:stream.write(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(path=str(target),bytes=target.stat().st_size,sha256=sha(target),sourceFiles=len(entries))))
