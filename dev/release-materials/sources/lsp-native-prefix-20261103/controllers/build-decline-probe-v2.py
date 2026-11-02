from pathlib import Path
import json,subprocess,hashlib,shutil
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;B=O/'full-v1';P=O/'decline-probe-v2';P.mkdir()
b=json.loads((B/'build-result.json').read_text());commands=[]
def run(n,c):
 r=subprocess.run(c,cwd=P,capture_output=True,timeout=180);(P/(n+'.log')).write_bytes(r.stdout+r.stderr);commands.append(dict(name=n,command=c,exit=r.returncode));assert not r.returncode,(n,r.stderr[-1400:])
shadow=P/'analysis';shadow.mkdir()
q=R/'ide/src/analysis/frontend_sem_raw.inc';s=q.read_text();start=s.index('static int xlsSemRawCheckFunc(');end=s.index('\nsize_t xlsFrontendSemRawMemoBytes',start)
body=s[start:end].replace('goto done;', '{fprintf(stderr,"XLS_RAW_DECLINE line=%d compatible=%d start=%u/%u types=%u/%u\\n",__LINE__,build ? build->compatible : -1,entry ? entry->start : 0,old ? old->start : 0,entry ? entry->typeStart : 0,old ? old->typeStart : 0);goto done;}')
(shadow/q.name).write_text(s[:start]+body+s[end:],encoding='utf8')
q=R/'ide/src/analysis/frontend_sem_final.inc';s=q.read_text();s=s.replace('/* After its original private-header walk;', '#define xlsSemNativeFailure(b) (fprintf(stderr,"XLS_FINAL_DECLINE line=%d\\n",__LINE__),xlsSemNativeFailure(b))\n/* After its original private-header walk;',1);(shadow/q.name).write_text(s,encoding='utf8')
q=R/'ide/src/analysis/frontend_sem_body.inc';s=q.read_text();start=s.index('static int xlsSemBodyPrepareCount(');end=s.index('\nstatic int xlsSemBodyPrepare(',start)
body=s[start:end].replace('return 0;', '{fprintf(stderr,"XLS_BODY_DECLINE line=%d oldPositions=%zu\\n",__LINE__,old->positions.Count);return 0;}')
(shadow/q.name).write_text(s[:start]+body+s[end:],encoding='utf8')
c=list(next(c for c in b['commands'] if '-c' in c and c[-1]==str(B/'xl6_sem.o')));c.insert(1,'-I'+str(P));c[-1]=str(P/'xl6_sem.o');run('sem',c)
shutil.copyfile(B/'xlang-analysis-core.a',P/'xlang-analysis-core.a');run('archive',[str(Path(b['compiler']).with_name('ar.exe')),'rcs',str(P/'xlang-analysis-core.a'),str(P/'xl6_sem.o')])
base=json.loads((O/'baseline-v2/build-result.json').read_text());A=O/'baseline-v2';c=list(next(c for c in base['commands'] if c[-1]==str(A/'xlang-ls.exe')));c=[str(P/'xlang-analysis-core.a') if v==str(A/'xlang-analysis-core.a') else v for v in c];c[-1]=str(P/'xlang-ls.exe');run('link',c)
(P/'result.json').write_text(json.dumps(dict(scope='Private declaration-retry diagnostics;not acceptance or production',commands=commands),indent=2)+'\n');print('decline probe built')
