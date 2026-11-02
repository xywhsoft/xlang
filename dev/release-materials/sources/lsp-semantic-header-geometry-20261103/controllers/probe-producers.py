from pathlib import Path
import json,subprocess
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'focused-v4';P=O/'producer-probe-v1';P.mkdir();b=json.loads((B/'result.json').read_text())
folder=P/'shadow'/'analysis';folder.mkdir(parents=True)
s=(R/'ide/src/analysis/frontend_sem_proof.inc').read_text()
s=s.replace('    /* Original bound global call/', '    if(node && (node->kind==XL6_AST_CALL || node->kind==XL6_AST_NAME))fprintf(stderr,"ORIGIN kind=%u sym=%u target=%u floor=%u call=%u eligible=%d count=%zu\\n",node->kind,node->semanticSymbolId,node->semanticTargetSymbolId,unit->symbols.floor,node->semanticCallKind,xlsSemGeometryCallable(build->memo->geometry,node->semanticTargetSymbolId),build->memo->geometry ? build->memo->geometry->callables.Count : 0);\n    /* Original bound global call/')
(folder/'frontend_sem_proof.inc').write_text(s)
s=(R/'ide/src/analysis/frontend_sem.inc').read_text()
s=s.replace('!xlsSemGeometryPosition(build->baseline->geometry,build->memo->geometry,&positioned,apply->geometryOrigin,&mapped))return 0;', '!xlsSemGeometryPosition(build->baseline->geometry,build->memo->geometry,&positioned,apply->geometryOrigin,&mapped)){fprintf(stderr,"DECLINE origin=%u path=%s tuple=%d:%d offset=%zu target=%zu compatible=%d track=%d\\n",apply->geometryOrigin,path,positioned.line,positioned.column,(size_t)positioned.range.startOffset,build && build->memo->geometry ? build->memo->geometry->callables.Count : 0,build ? build->compatible : 0,build ? build->memo->trackInputs : 0);return 0;}')
(folder/'frontend_sem.inc').write_text(s)
s=(R/'ide/tests/analysis_native_borrow_test.c').read_text();a=s.index('    xrtAtomic32Init(&checks,0);if(!editing())');e=s.index('    printf("analysis_native_borrow_test passed:',a);s=s[:a]+'    xrtAtomic32Init(&checks,0);if(!geometryEdits(0))return 1;\n'+s[e:];(P/'test.c').write_text(s)
commands=[]
def run(n,c):
 v=subprocess.run(c,cwd=P,capture_output=True,timeout=180);(P/(n+'.log')).write_bytes(v.stdout+v.stderr);commands.append(dict(name=n,command=c,exit=v.returncode));(P/'result.json').write_text(json.dumps(commands,indent=2));print(n,v.returncode,flush=True);assert not v.returncode or n=='run',v.stderr[-1500:]
c=list(next(row['command'] for row in b['commands'] if row['name']=='xl6_sem'));c.insert(1,'-I'+str(folder.parent));c[-1]=str(P/'sem.o');run('sem',c)
c=list(next(row['command'] for row in b['commands'] if row['name']=='analysis_native_borrow_test'));c[c.index('-c')+1]=str(P/'test.c');c[-1]=str(P/'test.o');run('test',c)
c=list(next(row['command'] for row in b['commands'] if row['name']=='analysis_native_borrow_test-link'));c.insert(2,str(P/'sem.o'));c=[str(P/'test.o') if v==str(B/'analysis_native_borrow_test.o') else v for v in c];c[-1]=str(P/'test.exe');run('link',c);run('run',[str(P/'test.exe')])
