from pathlib import Path
import json,hashlib,subprocess
R=Path('D:/GIT/x-lang');O=Path(__file__).resolve().parent;P=O/'dense-red-v2';P.mkdir();seed=json.loads((O/'source-base.json').read_text());B=Path(seed['baseline']);b=json.loads((B/'build-result.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
text=(O/'dense-red-v1/test.c').read_text()
seam='    left=xlsSemanticCheckJson(result,NULL);right=xlsSemanticCheckJson(original,NULL);CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);'
replacement='''    left=xlsSemanticCheckJson(result,NULL);right=xlsSemanticCheckJson(original,NULL);
    /* The unchanged legacy compatibility dumper stops at65536 nodes.
     * Dense cases compare EVERY typed fact and full AST/IR above,and the
     * complete original source/diagnostic report below. Preserve refusal. */
    if(xlsSemanticIndexCount(a)>65536u){CHECK(!left && !right);printf("legacy-report-refused:%zu facts\\n",xlsSemanticIndexCount(a));}
    else CHECK(left && right && !strcmp(left,right));
    xrtFree(left);xrtFree(right);left=xlsSemanticCheckReportJson(result,NULL);right=xlsSemanticCheckReportJson(original,NULL);
    CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);'''
assert text.count(seam)==1;text=text.replace(seam,replacement,1);p=P/'test.c';p.write_text(text,newline='\n')
record=dict(status='running',accepted=False,commands=[],source=str(p),sha256=sha(p))
def save():(P/'result.json').write_text(json.dumps(record,indent=2)+'\n')
try:
 for name in ['compile','link','run']:
  if name=='compile':
   c=list(next(c for c in b['commands'] if '-c' in c and str(R/'ide/tests/analysis_index_canonical_test.c') in c));c.insert(1,'-I'+str(R/'ide/tests'));c[c.index('-c')+1]=str(p);c[-1]=str(P/'test.o')
  elif name=='link':
   c=list(next(c for c in b['commands'] if c[-1]==str(B/'analysis_index_canonical_test.exe')));c[1]=str(P/'test.o');c[-1]=str(P/'test.exe')
  else:c=[str(P/'test.exe')]
  v=subprocess.run(c,cwd=P,capture_output=True,timeout=240);log=P/(name+'.log');log.write_bytes(v.stdout+v.stderr);record['commands'].append(dict(name=name,command=c,exit=v.returncode,log=str(log),sha256=sha(log)));save()
  if name!='run':assert not v.returncode,v.stderr[-1800:]
  else:
   assert v.returncode==1 and b'work.reused>=512' in v.stderr,v.stderr[-2000:]
   print('RED: complete current AST,all typed index facts,IR and source/diagnostic report match;expected512 reuse failed')
   print(v.stdout.decode('utf8').splitlines()[-1]);record['status']='expected-reuse-failure'
except BaseException as e:record.update(status='failed',error=repr(e));raise
finally:save()
