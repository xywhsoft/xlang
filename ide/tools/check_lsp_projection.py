"""Current projected-record analysis/navigation/completion over real stdio.

The compiler is absent. Source remains unsaved and native code is never run.
"""
from pathlib import Path
import argparse,hashlib,json
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

SOURCE='\ufeff// 中文😀\r\nextern "NativeRecord" struct Row @header("record.h") @c_projection()\r\n c_int value\r\nend\r\nfunc read(Row row) as c_int\r\n return row.value\r\nend\r\nfunc main(...) as int\r\n return 0\r\nend\r\n'


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--observe-legacy',action='store_true');a=p.parse_args();server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);root=out/'投影😀';root.mkdir();entry=root/'main.xl';entry.write_bytes(b'// live source deliberately differs\n');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(p):sha(p)for p in (server,entry,Path(__file__))};c=AutomaticClient(server,out/'absent/xl.exe',root);cases=[];report=dict(status='failed',cases=cases,protected=protected,scope=__doc__)
 def done(name):cases.append(dict(name=name,passed=True))
 def check(version):return c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=version))
 def params(text,needle='row.value',offset=4):return dict(textDocument=dict(uri=entry.as_uri()),position=position(text,text.index(needle)+offset))
 try:
  c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False)),1);reply,_=c.reply(1);assert 'error'not in reply,reply;c.send('initialized',{});c.open(entry,SOURCE,1);result=check(1)
  if a.observe_legacy:
   assert not result['ok'] and result['count'],result;done('previous-cohort-rejects-projected-record-source');report['status']='passed';return
  assert result['ok'],result;done('complete-unsaved-projected-record-native-check')
  at=SOURCE.index('value');expected=dict(uri=entry.as_uri(),range=dict(start=position(SOURCE,at),end=position(SOURCE,at+5)));assert c.call('textDocument/definition',params(SOURCE))==expected
  hover=c.call('textDocument/hover',params(SOURCE));assert hover and hover['contents']['value'],hover
  request=params(SOURCE);request['context']=dict(includeDeclaration=True);refs=c.call('textDocument/references',request);assert len(refs)==2 and expected in refs,refs;done('projected-record-field-native-binding-and-references')
  incomplete=SOURCE.replace('row.value','row.');c.change(entry,incomplete,2);request=params(incomplete,'row.',4);request['context']=dict(triggerKind=2,triggerCharacter='.');completion=c.call('textDocument/completion',request);items=completion.get('items',[])if isinstance(completion,dict)else completion;assert any(v['label']=='value'for v in items),completion;done('incomplete-unsaved-projected-member-completion')
  bad=SOURCE.replace('c_int value','string value');c.change(entry,bad,3);result=check(3);assert not result['ok'] and any('projected'in x['message']for x in result['diagnostics']),result;assert c.call('textDocument/definition',params(bad))is None;done('non-POD-field-rejects-and-invalidates-old-binding')
  c.change(entry,SOURCE,4);assert check(4)['ok'];assert c.call('textDocument/definition',params(SOURCE))==expected;done('recovered-buffer-rebinds-native-field')
  foreign=SOURCE+'extern "consume" @header("record.h") @no_raise func consume(Row row) as c_int\r\n';c.change(entry,foreign,5);result=check(5);assert not result['ok'] and any('projected'in x['message']for x in result['diagnostics']),result;done('projected-values-do-not-invent-an-implicit-C-parameter-ABI')
  corpus=Path(__file__).resolve().parents[2]/'test/cases/native_sequence_projection.xl';protected[str(corpus)]=sha(corpus);native=corpus.read_text(encoding='utf-8');c.change(entry,native,6);result=check(6);assert result['ok'],result;done('current-native-owned-sequence-nested-projection-corpus-analysis-only')
  def fields(text,version):
   c.change(entry,text,version);request=params(text,'row.',4);request['context']=dict(triggerKind=2,triggerCharacter='.');result=c.call('textDocument/completion',request);assert result.get('available'),result;return {item['label']for item in result['items']}
  plain=SOURCE.replace(' @c_projection()','').replace('row.value','row.');assert 'value'in fields(plain,7);done('exact-C-record-fields-use-the-same-native-collector')
  union='union Row\n int value\nend\nfunc read(Row row) as int\n return row.\nend\nfunc main(...) as int\n return 0\nend\n';assert 'value'in fields(union,8);done('ordinary-aggregate-fields-use-native-declared-types')
  renamed=SOURCE.replace('c_int value','c_int value @c_name("actualValue")').replace('row.value','row.');labels=fields(renamed,9);assert 'value'in labels and 'actualValue'not in labels,labels;done('completion-preserves-language-field-name-over-native-backing-name')
  report['status']='passed'
 finally:
  c.stop();(out/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
  for path,h in protected.items():assert sha(path)==h,path
  assert not(out/'absent/xl.exe').exists();(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print('PASS current projected-record stdio',len(cases),'cases',flush=True)


if __name__=='__main__':main()
