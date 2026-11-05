from pathlib import Path
import sys,re,json,hashlib
import argparse
parser=argparse.ArgumentParser(description='Generate audited default Model A direct declarations, exact POD/constant inventory and explicit C interoperability boundary')
parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--native-root',type=Path,required=True,help='Exact frozen Native source whose xui_document.h is the authority')
args=parser.parse_args();r=args.root.resolve();native=args.native_root.resolve();sys.path.insert(0,str(r/'tools'));import check_xge3_bindings as inv
h=inv.header_inventory(native/'include/xui_document.h');assert not h['parseErrors'];types={t['name']:t['type'] for t in h['types']};records={};primitive=dict(inv.SCALARS,xui_doc_node_id='uint64')
models={'xui_document':('Document','xui_document_t','xuiDocument','Document'), 'xui_document_snapshot':('DocumentSnapshot','xui_doc_snapshot_t','xuiDocumentSnapshot','DocumentSnapshot'),'xui_document_change_set':('DocumentChangeSet','xui_doc_change_set_t','xuiDocumentChangeSet','DocumentChangeSet'),'xui_document_prepare':('DocumentPrepare','xui_doc_prepare_t','xuiDocumentPrepare','DocumentPrepare'),'xui_document_fragment':('DocumentFragment','xui_doc_fragment_t','xuiDocumentFragment','DocumentFragment'),'xui_document_source_index':('DocumentSourceIndex','xui_doc_source_index_t','xuiDocumentSourceIndex','DocumentSourceIndex'),'xui_document_transaction':('DocumentTransaction','xui_doc_transaction_t','xuiDocumentTxn','DocumentTxn')}
reserved=set(re.findall(r'\{"([A-Za-z_]\w*)", XL6_KW_', (r/'src/lexer/xl6_lexer_gen.c').read_text(encoding='utf-8')))
def named(t):return ' '.join(t.get('names',[])) if t.get('kind')=='named' else None
def value(t,seen=()):
 k=t['kind']
 if k=='named':
  n=named(t)
  if n in primitive:return primitive[n]
  if n in records:return records[n]
  if n in types and n not in seen and types[n]['kind'] not in ('struct','union'):return value(types[n],(*seen,n))
 if k=='array':return value(t['of'],seen)
 raise ValueError('non-POD '+str(t))
pending={n:t for n,t in types.items() if t['kind']=='struct' and t.get('fields') is not None};recordLines=['// Pointer-free exact C records from frozen xui_document.h. Fixed char arrays are C bytes.']
while pending:
 changed=False
 for n,t in list(pending.items()):
  try:
   fields=[]
   for f in t['fields']:
    pt=value(f['type']);suffix=''
    if f['type']['kind']=='array':
     extent=f['type']['extent'];assert str(extent).isdigit();suffix='['+str(extent)+']'
    fields.append(' '+pt+' '+f['name']+suffix)
  except (ValueError,AssertionError,KeyError):continue
  ln='Document'+''.join(x.title() for x in n.removeprefix('xui_doc_').removesuffix('_t').split('_'))
  if ln=='DocumentTxnDesc':ln='DocumentTransactionDesc'
  records[n]=ln;recordLines+=['export extern "'+n+'" struct '+ln+' @header("xge3_runtime.h")',*fields,'end',''];del pending[n];changed=True
 if not changed:break
assert len(records)==26,(len(records),pending.keys())
known={'UINT64_MAX':(1<<64)-1};constantRows=[]
for enum in h['enums']:
 v=-1
 for e in enum['type']['values']:
  n=e['name'];v=inv.integer(e['expression'],known) if e['expression'] else v+1;known[n]=v;constantRows.append((n,v))
for m in h['constants']:
 n=m['name'];exp=re.sub(r'UINT64_C\(([^)]+)\)',r'\1',m['expression']);v=inv.integer(exp,known);known[n]=v;constantRows.append((n,v))
for n,v in constantRows:recordLines.append('export const '+('uint64' if n in ('XUI_DOCUMENT_ROOT','XUI_DOCUMENT_APPEND') or v>2147483647 else 'c_int')+' '+n+' = '+str(v))

classes={v[0]:[] for v in models.values()};mapped={};reasons={};functions=[]
def method(nativeName,owner=None):
 prefix=models[owner][2] if owner else 'xuiDocument'
 suffix=nativeName[len(prefix):] if nativeName.startswith(prefix) else nativeName.removeprefix('xuiDocument')
 if suffix.startswith('Get'):suffix=suffix[3:]
 if suffix.startswith('ShareOwned'):suffix='Share'
 if suffix.startswith('AcquireSnapshot'):suffix='Snapshot'
 return suffix[:1].lower()+suffix[1:]
def pname(n):return n+'Value' if not n or n in reserved else n
intValue={'xuiDocumentGetProfile','xuiDocumentGetMarkdownDialect','xuiDocumentIsDirty','xuiDocumentCanUndo','xuiDocumentCanRedo','xuiDocumentHasPrepare','xuiDocumentGetNotificationFailureStatus'}
exclude=re.compile(r'(?:AddRef|Release(?:Owner)?Checked|Ownership|Retain|Release)$')
for f in h['functions']:
 name=f['name'];sig=f['type'];ps=sig['parameters'];first=named(ps[0]['type']) if ps else None;owner=first if first in models else None
 if not owner:reasons[name]='No owned-model receiver; handled by explicit factory or C interoperability';continue
 if exclude.search(name) or 'ModelOwnership' in name:reasons[name]='Native lifecycle/metadata infrastructure; private policy or explicit C only';continue
 if name in ('xuiDocumentTxnAbort',):pass
 if name=='xuiDocumentSubscribe':reasons[name]='Raw callback installer; use SubscribeOwned';continue
 if name in ('xuiDocumentSnapshotAnalyzeMarkdownConversion','xuiDocumentSnapshotFind','xuiDocumentSnapshotFindEx'):
  reasons[name]='Caller output arrays require a checked writable typed-array output contract; never a single POD plus arbitrary capacity';continue
 if name=='xuiDocumentUnsubscribe':reasons[name]='Legacy void outcome; use UnsubscribeChecked';continue
 attrs=['@no_raise'];params=[];outs=[];outtypes=[];outputSizes=[];consumed=set();rootOut=None;bad=None
 try:
  for physical,p in enumerate(ps[1:],1):
   if physical in consumed:continue
   t=p['type'];n=named(t);pn=pname(p['name'])
   if n in models:params.append(models[n][0]+('?' if name=='xuiDocumentPrepareStreamSource' and p['name']=='previous' else '')+' '+pn);continue
   if t['kind']=='pointer':
    target=t['to'];tn=named(target);const='const' in target.get('qualifiers',[])
    if target['kind']=='named' and not const and tn in models:
     if rootOut:raise ValueError('multiple owning model outputs require tuple admission')
     rootOut=(physical,tn);outs.append(physical);outtypes.append(models[tn][0]);continue
    if physical+1<len(ps) and target['kind']=='named' and tn in ('char','void') and named(ps[physical+1]['type'])=='uint64_t':
     countName=ps[physical+1]['name'];assert countName in ('bytes','capacity','label_bytes','initial_bytes') or countName.endswith('_bytes')
     attrs.append('@c_span('+str(len(params))+',"'+('const ' if const else '')+tn+'*","size_t")');params.append(('string' if const and tn=='char' else 'bytes' if const else 'Buffer')+' '+pn);consumed.add(physical+1);continue
    if const and tn=='char':attrs.append('@c_cstr('+str(len(params))+')');params.append('string '+pn);continue
    if const and tn in records:attrs.append('@c_ref('+str(len(params))+')');params.append(records[tn]+' '+pn);continue
    if not const and tn in records:
     outs.append(physical);outtypes.append(records[tn]);rt=types[tn]
     if rt['fields'] and rt['fields'][0]['name']=='iSize':outputSizes.append(physical)
     continue
    if not const and tn in primitive and tn!='void':outs.append(physical);outtypes.append(primitive[tn]);continue
    raise ValueError('Pointer/record/callback requires a safe explicit contract')
   params.append(value(t)+' '+pn)
  returns=value(sig['returns'])
  if outs:
   if returns!='c_int':raise ValueError('unreviewed output status')
   if rootOut:
    if len(outs)!=1:raise ValueError('mixed owning and POD output')
    attrs+=['@c_result("owned_view")','@factory('+str(outs[0])+',0,rollback'+models[rootOut[1]][0]+')']
   elif len(outs)==1:attrs+=['@c_output('+str(outs[0])+',0)']
   else:attrs+=['@c_outputs('+','.join(map(str,outs))+')','@c_status(0)']
   attrs+=['@c_output_size('+str(i)+',"iSize")' for i in outputSizes];returns=outtypes[0] if len(outs)==1 else '('+','.join(outtypes)+')'
  elif returns=='c_int' and name not in intValue:attrs.append('@c_result("status",0)');returns=None
  mn=method(name,owner);line=' extern "'+name+'" '+' '.join(attrs)+' func '+mn+'('+','.join(params)+')'+(' as '+returns if returns else '')
  if any(('func '+mn+'(') in old for old in classes[models[owner][0]]):raise ValueError('logical name conflict needs explicit review')
  classes[models[owner][0]].append(line);mapped[name]=models[owner][0]+'.'+mn
 except (ValueError,AssertionError,TypeError) as e:reasons[name]=str(e)
# explicit safe descriptor constructors; callbacks/custom allocators remain zero.
functions.append('export extern "xuiDocumentCreateOwned" @header("xge3_runtime.h") @no_raise @c_result("owned_view") @factory(1,0,rollbackDocument) @c_pack(0,"xui_doc_desc_t",0,8) @c_pack_size(0,"iSize","uint32_t") '+ ' '.join('@c_pack_field(0,"'+field+'",'+str(i)+',"value")' for i,field in enumerate(['iProfile','iMarkdownDialect','iHistoryLimit','iHistoryMaxBytes','bDisableHistory','iMaxTextBytes','iMaxNodes','iMaxSyntaxRecords']))+' func createDocument(c_uint profile=1,c_uint markdownDialect=0,c_uint historyLimit=0,uint64 historyMaxBytes=0,c_int disableHistory=0,uint64 maxTextBytes=0,uint64 maxNodes=0,uint64 maxSyntaxRecords=0) as Document')
mapped['xuiDocumentCreateOwned']='createDocument';reasons.pop('xuiDocumentCreateOwned',None)
# Descriptor parameter alternatives are built by one generic stack pack.
manual=[('xuiDocumentBeginTransaction','Document','beginTransaction','xui_doc_txn_desc_t',1,2,['iDomain','iBaseRevision','iOrigin','iGroup'],'c_uint domain=1,uint64 baseRevision=0,uint64 origin=0,uint64 group=0','DocumentTransaction'),('xuiDocumentSnapshotCreateSourceIndex','DocumentSnapshot','createSourceIndex','xui_doc_source_index_desc_t',1,2,['iMaxWorkingBytes'],'uint64 maxWorkingBytes=0','DocumentSourceIndex')]
for nativeName,cl,mn,ct,slot,output,fields,params,result in manual:
 classes[cl]=[line for line in classes[cl] if not ('extern "'+nativeName+'"' in line)]
 attrs='@no_raise @c_result("owned_view") @factory('+str(output)+',0,rollback'+result+') @c_pack('+str(slot)+',"'+ct+'",0,'+str(len(fields))+') @c_pack_size('+str(slot)+',"iSize","uint32_t") '+' '.join('@c_pack_field('+str(slot)+',"'+field+'",'+str(i)+',"value")' for i,field in enumerate(fields))
 classes[cl].append(' extern "'+nativeName+'" '+attrs+' func '+mn+'('+params+') as '+result);mapped[nativeName]=cl+'.'+mn;reasons.pop(nativeName,None)
# The streaming descriptor is packed, so callers never guess native sizeof.
classes['Document']=[line for line in classes['Document'] if 'extern "xuiDocumentPrepareStreamSource"' not in line]
classes['Document'].append(' extern "xuiDocumentPrepareStreamSource" @no_raise @c_result("owned_view") @factory(6,0,rollbackDocumentPrepare) @c_pack(2,"xui_doc_txn_desc_t",1,4) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"iDomain",1,"value") @c_pack_field(2,"iBaseRevision",2,"value") @c_pack_field(2,"iOrigin",3,"value") @c_pack_field(2,"iGroup",4,"value") @c_span(5,"const char*","size_t") func prepareStreamSource(DocumentPrepare? previous,c_uint domain=2,uint64 baseRevision=0,uint64 origin=0,uint64 group=0,string utf8="",c_int finalChunk=1) as DocumentPrepare')

# Pointer-bearing native records project only their direct POD members.
for nativeName,cl,mn,ct,slot,params,fields in [
 ('xuiDocumentSnapshotGetNode','DocumentSnapshot','nodeInfo','xui_doc_node_info_t',2,'uint64 node',['iKind','iId','iParentId','iChildCount','iTextBytes','iSourceStart','iSourceEnd','bSourceExact','iSyntaxStart','iSyntaxEnd','iSourceSegmentCount','iExtensionPayloadBytes','iExtensionVersion','bExtensionRequired']),
 ('xuiDocumentTxnGetNode','DocumentTransaction','nodeInfo','xui_doc_node_info_t',2,'uint64 node',['iKind','iId','iParentId','iChildCount','iTextBytes','iSourceStart','iSourceEnd','bSourceExact','iSyntaxStart','iSyntaxEnd','iSourceSegmentCount','iExtensionPayloadBytes','iExtensionVersion','bExtensionRequired']),
 ('xuiDocumentChangeSetGetInfo','DocumentChangeSet','info','xui_doc_change_info_t',1,'',['iFlags','iBeforeRevision','iAfterRevision','iOrigin','iInputGeneration','iOperationCount','bUndo','iDomain']),
 ('xuiDocumentSnapshotQueryLink','DocumentSnapshot','linkState','xui_doc_link_info_t',2,'DocumentRange range',['bLinked','bMixed'])]:
 fieldTypes={f['name']:value(f['type']) for f in types[ct]['fields'] if f['name'] in fields}
 attrs=['@no_raise','@c_output_record('+str(slot)+',"'+ct+'","borrowed")','@c_output_size('+str(slot)+',"iSize")','@c_status(0)']
 if 'DocumentRange' in params:attrs+=['@c_ref(0)']
 attrs+=['@c_output_field('+str(i)+',"'+field+'")' for i,field in enumerate(fields)]
 classes[cl].append(' extern "'+nativeName+'" '+' '.join(attrs)+' func '+mn+'('+params+') as ('+','.join(fieldTypes[f] for f in fields)+')')
 mapped[nativeName]=cl+'.'+mn;reasons.pop(nativeName,None)
# Simple node/image descriptors copy all named inputs before returning. Native attributes default to zero.
for nativeName,mn,slot,outSlot,params,fields,outputs in [
 ('xuiDocumentTxnInsertNode','insertNode',3,[4],'uint64 parent,uint64 childIndex,c_uint kind,string text="",string? resource=null,string? info=null,string? title=null,string? linkTarget=null,string? linkTitle=null', [('iKind',2,'value'),('sText',3,'text_data'),('iTextBytes',3,'text_size'),('sResource',4,'c_string'),('sInfo',5,'c_string'),('sTitle',6,'c_string'),('sLinkTarget',7,'c_string'),('sLinkTitle',8,'c_string')], ['uint64']),
 ('xuiDocumentTxnInsertImage','insertImage',2,[3,4],'DocumentRange range,string resource,string alt="",string? title=null,string? linkTarget=null,string? linkTitle=null,c_float width=0,c_float height=0', [('sResource',1,'c_string'),('sAlt',2,'text_data'),('iAltBytes',2,'text_size'),('sTitle',3,'c_string'),('sLinkTarget',4,'c_string'),('sLinkTitle',5,'c_string'),('fWidth',6,'value'),('fHeight',7,'value')], ['uint64','DocumentPosition']),
 ('xuiDocumentTxnUpdateImage','updateImage',2,[],'uint64 image,string resource,string alt="",string? title=null,string? linkTarget=null,string? linkTitle=null,c_float width=0,c_float height=0', [('sResource',1,'c_string'),('sAlt',2,'text_data'),('iAltBytes',2,'text_size'),('sTitle',3,'c_string'),('sLinkTarget',4,'c_string'),('sLinkTitle',5,'c_string'),('fWidth',6,'value'),('fHeight',7,'value')], [])]:
 ct='xui_doc_node_desc_t' if mn=='insertNode' else 'xui_doc_image_desc_t';first=2 if mn=='insertNode' else 1;count=7
 attrs=['@no_raise','@c_pack('+str(slot)+',"'+ct+'",'+str(first)+','+str(count)+')','@c_pack_size('+str(slot)+',"iSize","uint32_t")']
 if mn=='insertImage':attrs+=['@c_ref(0)']
 attrs+=['@c_pack_field('+str(slot)+',"'+f+'",'+str(i)+',"'+kind+'"'+(',"size_t"' if kind=='text_size' else '')+')' for f,i,kind in fields]
 if len(outSlot)==1:attrs+=['@c_output('+str(outSlot[0])+',0)']
 elif outSlot:attrs+=['@c_outputs('+','.join(map(str,outSlot))+')','@c_status(0)','@c_output_size(4,"iSize")']
 else:attrs+=['@c_result("status",0)']
 result=(' as '+outputs[0]) if len(outputs)==1 else (' as ('+','.join(outputs)+')') if outputs else ''
 classes['DocumentTransaction'].append(' extern "'+nativeName+'" '+' '.join(attrs)+' func '+mn+'('+params+')'+result)
 mapped[nativeName]='DocumentTransaction.'+mn;reasons.pop(nativeName,None)

# Whole native attributes are a flat descriptor pack; fixed FontFamily is reset to empty.
fields=[('iMarks','c_uint marks=0'),('iFlags','c_uint flags=0'),('iHeadingLevel','c_uint headingLevel=0'),('iAlignment','c_uint alignment=0'),('iRowSpan','c_uint rowSpan=0'),('iColumnSpan','c_uint columnSpan=0'),('iListStart','uint64 listStart=0'),('iTextColor','c_uint textColor=0'),('iBackgroundColor','c_uint backgroundColor=0'),('fFontSize','c_float fontSize=0'),('fWidth','c_float width=0'),('fHeight','c_float height=0'),('fParagraphSpacing','c_float paragraphSpacing=0'),('sLanguage','string? language=null')]
attrs=['@no_raise','@c_result("status",0)','@c_pack(2,"xui_doc_attributes_t",1,14)']+['@c_pack_field(2,"'+f+'",'+str(i+1)+',"'+('c_string' if f=='sLanguage' else 'value')+'")' for i,(f,_) in enumerate(fields)]
classes['DocumentTransaction'].append(' extern "xuiDocumentTxnSetAttributes" '+' '.join(attrs)+' func setAttributesColumns(uint64 node,'+','.join(p for _,p in fields)+')')
mapped['xuiDocumentTxnSetAttributes']='DocumentTransaction.setAttributesColumns';reasons.pop('xuiDocumentTxnSetAttributes',None)

# Exact int-result retained notification; independent callback epochs.
classes['Document'].append(' extern "xuiDocumentSubscribeOwned" @no_raise @c_owned_callback(0,0,"xui_callback_owner_t",-16) @c_output(4,0) func subscribe(DocumentChangeProc callback) as uint64')
mapped['xuiDocumentSubscribeOwned']='Document.subscribe';reasons.pop('xuiDocumentSubscribeOwned',None)
classes['DocumentSourceIndex'].append(' extern "xuiDocumentSourceIndexVisitBindings" @no_raise @c_sync_callback(2,-1,-16,"first") @c_output(5,0) func visitBindings(uint64 start,uint64 endValue,DocumentSourceBindingProc callback) as uint64')
mapped['xuiDocumentSourceIndexVisitBindings']='DocumentSourceIndex.visitBindings';reasons.pop('xuiDocumentSourceIndexVisitBindings',None)
# Legacy methods above never expose an untracked consuming release.
lines=['// Default-provenance models only. Every ordinary method is one direct native call.', '// Raw allocator/observer installers and pointer-bearing value records stay C interoperability.']
for nativeName,(cl,tag,prefix,policy) in models.items():
 lines+=['export extern "struct '+tag+'*" class '+cl+' @header("xge3_runtime.h") @lifetime("owned") @policy("xl6Xui3'+policy+'Policy") @domain("xl6Xui3DocumentModelDomainV1")',' native point handle',*classes[cl], ' extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()','end','extern "'+prefix+'ReleaseChecked" @header("xge3_runtime.h") @no_raise @c_result("status",0) func rollback'+cl+'('+cl+' value)','']
# Factories without descriptor/callback escape; native Owned entry preserves provenance.
for nativeName,logical in [('xuiDocumentFragmentImportHtmlOwned','importDocumentFragmentHtml'),('xuiDocumentFragmentDeserializeOwned','deserializeDocumentFragment')]:
 functions.append('export extern "'+nativeName+'" @header("xge3_runtime.h") @no_raise @c_span(0,"const char*","size_t") @c_result("owned_view") @factory(2,0,rollbackDocumentFragment) func '+logical+'('+('bytes' if nativeName=='xuiDocumentFragmentDeserializeOwned' else 'string')+' data) as DocumentFragment')
 mapped[nativeName]=logical;reasons.pop(nativeName,None)
# Each descriptor is stack-packed; allocator callbacks remain NULL.
fields=['iProfile','iMarkdownDialect','iHistoryLimit','iHistoryMaxBytes','bDisableHistory','iMaxTextBytes','iMaxNodes','iMaxSyntaxRecords']
config='c_uint profile=1,c_uint markdownDialect=0,c_uint historyLimit=0,uint64 historyMaxBytes=0,c_int disableHistory=0,uint64 maxTextBytes=0,uint64 maxNodes=0,uint64 maxSyntaxRecords=0'
for nativeName,logical,first,packSlot,outSlot,baseParameters in [('xuiDocumentOpenFileOwned','openDocument',2,2,4,'string path,c_uint format'),('xuiDocumentDeserializeOwned','deserializeDocument',0,0,3,'')]:
 attrs=['@no_raise','@c_result("owned_view")','@factory('+str(outSlot)+',0,rollbackDocument)']
 attrs+=['@c_cstr(0)'] if packSlot==2 else ['@c_span(8,"const char*","size_t")']
 attrs+=['@c_pack('+str(packSlot)+',"xui_doc_desc_t",'+str(first)+',8)','@c_pack_size('+str(packSlot)+',"iSize","uint32_t")']
 attrs+=['@c_pack_field('+str(packSlot)+',"'+field+'",'+str(first+i)+',"value")' for i,field in enumerate(fields)]
 functions.append('export extern "'+nativeName+'" @header("xge3_runtime.h") '+' '.join(attrs)+' func '+logical+'('+((baseParameters+',') if baseParameters else '')+config+(',uint64 maxFileBytes=0' if packSlot==2 else ',string data=""')+') as Document')
 mapped[nativeName]=logical;reasons.pop(nativeName,None)
for n in ('xui_document','xui_document_change_set'):

 cl,tag,prefix,policy=models[n];cb='Callback'+cl
 lines+=['export extern "struct '+tag+'*" class '+cb+' @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "'+prefix+'ShareOwned" @no_raise @c_result("owned_view") @factory(1,0,rollback'+cl+') func share() as '+cl,*[line for line in classes[cl] if any(('func '+mn+'(') in line for mn in (('identity','revision','profile','markdownDialect') if cl=='Document' else ('info',)))],'end','']
lines+=['export extern "const xui_doc_source_binding_t*" class CallbackDocumentSourceBinding @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "value" @c_deref get value() as DocumentSourceBinding','end','export typedef DocumentSourceBindingProc = func(uint64,CallbackDocumentSourceBinding) as c_int','']
lines+=['export typedef DocumentChangeProc = func(CallbackDocument,CallbackDocumentChangeSet) as c_int','',*functions,'']
src=r/'lib/bindings/xui/src';(src/'document_records.xl').write_text('\n'.join(recordLines)+'\n',encoding='utf-8',newline='\n');(src/'document.xl').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
report=dict(scope='Default-provenance Model A direct declaration coverage; declarations are not ABI/runtime qualification',authority=h['header'],authoritySha256=h['sha256'],roots={k:dict(logical=v[0],cName='struct '+v[1]+'*',policy='xl6Xui3'+v[3]+'Policy') for k,v in models.items()},records=records,pointerBearingRecords=list(pending),entries=[dict(native=f['name'],declaration=f['declaration'],line=f['line'],logical=mapped.get(f['name']),state='declared-needs-ABI' if f['name'] in mapped else 'explicit-C-interoperability',reason=reasons.get(f['name'])) for f in h['functions']],counts=dict(publicFunctions=len(h['functions']),declared=len(mapped),ownedRoots=7,pointerFreeRecords=len(records),constants=len(constantRows)))

replacements={
 'xuiDocumentSnapshotAnalyzeMarkdownConversion':'Writable caller output array; a single POD plus arbitrary capacity is unsafe. A checked typed-array output contract is not declared.',
 'xuiDocumentSnapshotFind':'Writable matches array plus capacity; nullable scope and complete array capacity must be represented together, not a single output record.',
 'xuiDocumentSnapshotFindEx':'Writable matches array plus capacity; same boundary as Find, with native regex flags.',
 'xuiDocumentPrepareSource':'Array of size-prefixed patch records with counted text pointers. Existing column packs do not initialize each iSize or map per-element text byte lengths.',
 'xuiDocumentPrepareContinueSource':'Same counted patch-array boundary. prepareStreamSource provides safe incremental UTF-8 append but does not replace arbitrary patches.',
 'xuiDocumentCreate':'Legacy raw/custom-allocator provenance; createDocument maps CreateOwned with default allocator only.',
 'xuiDocumentOpenFile':'Legacy raw/custom-allocator provenance; openDocument maps OpenFileOwned with default allocator only.',
 'xuiDocumentDeserialize':'Legacy raw/custom-allocator provenance; deserializeDocument maps DeserializeOwned with default allocator only.',
 'xuiDocumentFragmentImportHtml':'Legacy raw output; importDocumentFragmentHtml maps the real ImportHtmlOwned entry.',
 'xuiDocumentFragmentDeserialize':'Legacy raw output; deserializeDocumentFragment maps the real DeserializeOwned entry.',
 'xuiDocumentFreeBuffer':'Paired only with Native libc/default-allocator output buffers. xrtFree/take_cstr are incompatible.',
}
for entry in report['entries']:
 name=entry['native']
 if entry['logical']:entry['state']='declared-needs-ABI';continue
 if any(x in name for x in ('AddRef','ReleaseOwnerChecked','ReleaseChecked','Ownership','Retain','Release')):
  entry['state']='policy-or-legacy-C-infrastructure';entry['reason']='Real ownership metadata/cleanup is consumed by the private policy. Raw Retain/Release is not exposed as an extra language owner.'
 elif 'char** out' in entry['declaration']:
  entry['reason']='Producer-owned libc/default-allocator text + explicit byte count; must release through xuiDocumentFreeBuffer. No compatible allocator-aware owned-buffer output contract is declared.'
 if name in replacements:entry['reason']=replacements[name]
report['fields']={
 'xui_doc_node_info_t':dict(projectedPOD=['iKind','iId','iParentId','iChildCount','iTextBytes','iSourceStart','iSourceEnd','bSourceExact','iSyntaxStart','iSyntaxEnd','iSourceSegmentCount','iExtensionPayloadBytes','iExtensionVersion','bExtensionRequired'],notProjected=dict(tAttributes='Contains borrowed sLanguage plus fixed font-family bytes; not pointer-free POD.',sResource='Borrowed NUL text, unsupported output-record field conversion.',sInfo='Borrowed NUL text.',sTitle='Borrowed NUL text.',sLinkTarget='Borrowed NUL text.',sLinkTitle='Borrowed NUL text.'),safeTextAlternative='copyText/readText and copySource/readSource write into scoped Buffer storage.'),
 'xui_doc_change_info_t':dict(projectedPOD=['iFlags','iBeforeRevision','iAfterRevision','iOrigin','iInputGeneration','iOperationCount','bUndo','iDomain'],notProjected=dict(pOperations='Borrowed counted POD array; no implicit raw pointer exposure. NativeSequence currently requires distinct data/count output slots.')),
 'xui_doc_link_info_t':dict(projectedPOD=['bLinked','bMixed'],notProjected=dict(sUri='Borrowed NUL text; no output-record string-field conversion.',sTitle='Borrowed NUL text; no output-record string-field conversion.')),
 'xui_doc_attributes_t':dict(input='setAttributesColumns packs 13 scalar fields and nullable language; replaces the complete attributes value.',notProjected=dict(sFontFamily='Fixed char[64] is zeroed by this basic pack. Use range setTextStyle with a real DocumentTextStyle POD for a font family.')),
 'xui_doc_node_desc_t':dict(input='insertNode packs kind, counted text, resource/info/title/link fields; Native copies them before return.',notProjected=dict(tAttributes='Zero-initialized default attributes; adjust the inserted node with transaction methods.',pExtensionPayload='Use setExtensionPayload with checked bytes; no opaque pointer field.')),
 'xui_doc_image_desc_t':dict(input='insertImage/updateImage pack all public fields with counted alt text and nullable NUL strings.'),
 'xui_doc_source_index_desc_t':dict(input='createSourceIndex packs default allocator settings and maxWorkingBytes.',notProjected=dict(onCancel='Callback nested inside a packed descriptor is not synthesized as a persistent raw callback.',pCancelUser='No user pointer exposed.')),
 'xui_doc_desc_t':dict(input='Owned constructors pack size and all eight ordinary settings.',notProjected=dict(onAlloc='Custom allocator callback lifetime is C-only.',onFree='Must be paired with allocator, outlives all handles.',pAllocatorUser='No raw userdata exposed.')),
 'xui_doc_source_patch_t':dict(notProjected=dict(iSize='Per-element sizeof initialization needs a checked array-record field contract.',sText='Per-element counted string lifetime/byte length cannot be replaced by NUL scanning.',iTextBytes='Must be paired with its actual element text source.'))}
report['costs']=dict(nativeForwardingFunctions=0,descriptorPacks='Stack initialization and field assignments; counted inputs are pinned; NUL strings require validation/scans.',owningRoots='One resident control/code lease and real native owner per independent root; share/factories reserve before producer.',POD='Exact native values or stack output-record field copies; no arbitrary pointer projection.',buffers='Scope/pin/length checks; Native Copy performs its documented bytes copy; lengths remain unchanged.',callbacks='Native Owned capture ownership plus checked epoch admission; explicit share acquires a real separate owner.',overallZeroOverheadClaim=False)

(r/'docs/xge3-xui3-document-api-map.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(report['counts']);print('records pending',list(pending))
