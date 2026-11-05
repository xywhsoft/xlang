"""Bounded actual stdio diagnostics/completion and matching package analysis."""
from pathlib import Path
import argparse,hashlib,json,sys,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import checked
from check_lsp_analysis_configuration import LISTS,PROFILE
from check_language_definitions import position
p=argparse.ArgumentParser(description=__doc__)
for name in ('server','core','packages','out'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();server,core,packages,out=[getattr(a,n).resolve() for n in ('server','core','packages','out')];out.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs={str(p):sha(p) for p in [server,Path(__file__).resolve(),*[p for p in packages.rglob('*') if p.is_file() and p.suffix in ('.xl','.json')]]};report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,compilerInvoked=False,rows=[])
def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
cases=[('code-owner',(core/'test/cases/foreign_module_code_owner.xl').read_text(),True,''),('sequence',(core/'test/cases/native_sequence_contract.xl').read_text(),True,''),('bad-code-owner','extern "bad" @header("native.h") @c_code_owner(-1) func bad()\nfunc main(...) as int\n return 0\nend\n',False,''),('packages','import xui\nimport xge\nexport func inspect(xge.DataObject data) as int\n return int(data.formatCount)\nend\nfunc main(...) as int\n xui.Rect bounds=xui.Rect()\n bounds.fW=c_int(10)\n return int(bounds.fW)\nend\n',True,str(packages))]
for name,body,accepted,package_path in cases:
 folder=out/name;folder.mkdir();entry=folder/'main.xl';entry.write_text('// actual open overlay\n');c=AutomaticClient(server,folder/'absent/xl.exe',folder)
 try:
  lists={key:[] for key in LISTS};lists['members']=[entry.as_posix()]
  context=dict(schema=1,kind='project',project=(folder/'project.xlp').as_posix(),configuration='release',entry=entry.as_posix(),directory=folder.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath=package_path,lists=lists)
  c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,analysisConfiguration=dict(epoch=1,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath=package_path)))),1);reply,_=c.reply(1);assert 'error' not in reply,reply;c.send('initialized',{})
  for round in range(3):
   version=round+1;current=body+'\n// observation '+str(round)+'\n'
   if round:c.change(entry,current,version)
   else:c.open(entry,current,version)
   start=time.monotonic();result=checked(c,entry,version);elapsed=(time.monotonic()-start)*1000
   report['rows'].append(dict(case=name,round=round,elapsedMs=elapsed,result=result));save();assert result['ok']==accepted,result
   assert (result['count']==0 if accepted else result['count']>0),result
  # The transport's native completion path remains independently callable.
  text='func main(...) as int\n int localValue=3\n return loca\nend\n';c.change(entry,text,4)
  mark=text.index('loca\n')+4;reply=c.call('textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),version=4,position=position(text,mark)))
  items=reply.get('items',[]) if isinstance(reply,dict) else reply
  assert any(item.get('label')=='localValue' for item in items),reply
  report['rows'].append(dict(case=name,completion=True,count=len(items)));save()
 finally:
  stderr=c.stop();(folder/'transcript.json').write_text(json.dumps(c.transcript,indent=2)+'\n');assert not stderr and c.process.returncode==0,(stderr,c.process.returncode)
report.update(complete=True,inputsUnchanged=all(sha(name)==value for name,value in inputs.items()));report['passed']=report['inputsUnchanged'];save();print(len(report['rows']),'checks',report['passed'])
