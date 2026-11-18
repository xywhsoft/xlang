from pathlib import Path
import sys,re,json,hashlib
import argparse
parser=argparse.ArgumentParser(description='Generate audited default Model A direct declarations, exact POD/constant inventory and explicit C interoperability boundary')
parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--native-root',type=Path,required=True,help='Exact frozen Native source whose xui_document.h is the authority')
parser.add_argument('--check',action='store_true',help='Check generated declaration/record bytes without writing source')
args=parser.parse_args();r=args.root.resolve();native=args.native_root.resolve();sys.path.insert(0,str(r/'tools'));import check_xge3_bindings as inv
ui=inv.header_inventory(native/'include/xui.h');external_types={t['name']for t in ui['types']}
assert 'xui_callback_owner_t'in external_types
h=inv.header_inventory(native/'include/xui_document.h',external_types=external_types);assert not h['parseErrors'];types={t['name']:t['type'] for t in h['types']};records={};primitive=dict(inv.SCALARS,xui_doc_node_id='uint64')
models={'xui_document':('Document','xui_document_t','xuiDocument','Document'), 'xui_document_snapshot':('DocumentSnapshot','xui_doc_snapshot_t','xuiDocumentSnapshot','DocumentSnapshot'),'xui_document_change_set':('DocumentChangeSet','xui_doc_change_set_t','xuiDocumentChangeSet','DocumentChangeSet'),'xui_document_prepare':('DocumentPrepare','xui_doc_prepare_t','xuiDocumentPrepare','DocumentPrepare'),'xui_document_fragment':('DocumentFragment','xui_doc_fragment_t','xuiDocumentFragment','DocumentFragment'),'xui_document_source_index':('DocumentSourceIndex','xui_doc_source_index_t','xuiDocumentSourceIndex','DocumentSourceIndex'),'xui_document_transaction':('DocumentTransaction','xui_doc_transaction_t','xuiDocumentTxn','DocumentTxn')}
models.update({
 'xui_document_matches':('DocumentMatches','xui_doc_matches_t','xuiDocumentMatches','DocumentMatches'),
 'xui_document_markdown_losses':('DocumentMarkdownLosses','xui_doc_markdown_losses_t','xuiDocumentMarkdownLosses','DocumentMarkdownLosses'),
})
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
assert len(records)==27,(len(records),pending.keys())
known={'UINT64_MAX':(1<<64)-1};constantRows=[]
for enum in h['enums']:
 v=-1
 for e in enum['type']['values']:
  n=e['name'];v=inv.integer(e['expression'],known) if e['expression'] else v+1;known[n]=v;constantRows.append((n,v))
for m in h['constants']:
 if m['name']=='XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR':continue # Compiler/provider ABI promise, not a language option.
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
# Exact resident-XRT export producers. Text and bytes are separate admissions.
exportPositions={}
for cl,nativeName,mn,slots,params,extra in [
 ('DocumentSnapshot','xuiDocumentSerialize','serialize',(1,2),'',''),
 ('DocumentSnapshot','xuiDocumentExportHtml','exportHtml',(1,2),'',''),
 ('DocumentSnapshot','xuiDocumentSnapshotCopyPlainText','plainText',(1,2),'',''),
 ('DocumentSnapshot','xuiDocumentSnapshotCopyRange','range',(2,3),'DocumentRange range','@c_ref(0) '),
 ('DocumentSnapshot','xuiDocumentSnapshotCopyTableMatrix','tableMatrix',(6,7),'uint64 table,c_uint row,c_uint column,c_uint rows,c_uint columns',''),
 ('DocumentFragment','xuiDocumentFragmentSerialize','serialize',(1,2),'',''),
 ('DocumentFragment','xuiDocumentFragmentExportHtml','exportHtml',(1,2),'','')]:
 modes=[('bytes',mn)]if mn=='serialize'else [('bytes',mn+'Bytes'),('text',mn)]
 exported=[]
 for mode,name in modes:
  exported.append(' extern "'+nativeName+'" @header("xge3_document_xrt.h") @no_raise '+extra+'@c_output_'+mode+'('+str(slots[0])+','+str(slots[1])+',"char*","uint64_t","xrt",0) func '+name+'('+params+') as '+('string'if mode=='text'else'bytes'))
 position=exportPositions.get(cl,0);classes[cl][position:position]=exported;exportPositions[cl]=position+len(exported)
 mapped[nativeName]=cl+'.'+mn;reasons.pop(nativeName,None)
# Prefix length is logical-only; uint64_t is the actual native ABI.
for i,line in enumerate(classes['Document']):
 if 'extern "xuiDocumentLoadMarkdown"'in line:
  classes['Document'][i:i+1]=[' extern "xuiDocumentLoadMarkdown" @no_raise @c_span(0,"const char*","uint64_t",1) @c_result("status",0) func loadMarkdown('+kind+' utf8,int size=-1)'for kind in('string','bytes','Buffer')];break
# explicit safe descriptor constructors; callbacks/custom allocators remain zero.
functions.append('export extern "xuiDocumentCreateOwned" @header("xge3_runtime.h") @no_raise @c_result("owned_view") @factory(1,0,rollbackDocument) @c_pack(0,"xui_doc_desc_t",0,8) @c_pack_size(0,"iSize","uint32_t") '+ ' '.join('@c_pack_field(0,"'+field+'",'+str(i)+',"value")' for i,field in enumerate(['iProfile','iMarkdownDialect','iHistoryLimit','iHistoryMaxBytes','bDisableHistory','iMaxTextBytes','iMaxNodes','iMaxSyntaxRecords']))+' func createDocument(c_uint profile=1,c_uint markdownDialect=0,c_uint historyLimit=0,uint64 historyMaxBytes=0,c_int disableHistory=0,uint64 maxTextBytes=0,uint64 maxNodes=0,uint64 maxSyntaxRecords=0) as Document')
mapped['xuiDocumentCreateOwned']='createDocument';reasons.pop('xuiDocumentCreateOwned',None)
# Descriptor parameter alternatives are built by one generic stack pack.
manual=[('xuiDocumentBeginTransaction','Document','beginTransaction','xui_doc_txn_desc_t',1,2,['iDomain','iBaseRevision','iOrigin','iGroup'],'c_uint domain=1,uint64 baseRevision=0,uint64 origin=0,uint64 group=0','DocumentTransaction'),('xuiDocumentSnapshotCreateSourceIndex','DocumentSnapshot','createSourceIndex','xui_doc_source_index_desc_t',1,2,['iMaxWorkingBytes'],'uint64 maxWorkingBytes=0','DocumentSourceIndex')]
for nativeName,cl,mn,ct,slot,output,fields,params,result in manual:
 classes[cl]=[line for line in classes[cl] if not ('extern "'+nativeName+'"' in line)]
 attrs='@no_raise @c_result("owned_view") @factory('+str(output)+',0,rollback'+result+') @c_pack('+str(slot)+',"'+ct+'",0,'+str(len(fields))+') @c_pack_size('+str(slot)+',"iSize","uint32_t") '+' '.join('@c_pack_field('+str(slot)+',"'+field+'",'+str(i)+',"value")' for i,field in enumerate(fields))
 classes[cl].append(' extern "'+nativeName+'" '+attrs+' func '+mn+'('+params+') as '+result);mapped[nativeName]=cl+'.'+mn;reasons.pop(nativeName,None)
# Counted source patches keep one column pin for data and its true byte length.
# Named arguments allow the descriptor defaults before the patch columns.
for kind,suffix in [('string',''),('bytes','Bytes')]:
 name='xuiDocumentPrepareSource'
 attrs='@no_raise @c_result("owned_view") @factory(4,0,rollbackDocumentPrepare) @c_pack(1,"xui_doc_txn_desc_t",0,4) @c_pack_size(1,"iSize","uint32_t") @c_pack_field(1,"iDomain",0,"value") @c_pack_field(1,"iBaseRevision",1,"value") @c_pack_field(1,"iOrigin",2,"value") @c_pack_field(1,"iGroup",3,"value") @c_array_group(2,4,3,"uint64_t",7) @c_array_record(2,"xui_doc_source_patch_t","iSize","uint32_t") @c_array_field(2,"iStart",0,"value") @c_array_field(2,"iEnd",1,"value") @c_array_span_field(2,"sText","iTextBytes",2,"const char*","uint64_t")'
 classes['Document'].append(' extern "'+name+'" '+attrs+' func prepareSource'+suffix+'(c_uint domain=2,uint64 baseRevision=0,uint64 origin=0,uint64 group=0,array<uint64> starts=[],array<uint64> ends=[],array<'+kind+'> text=[],int size=-1) as DocumentPrepare')
 name='xuiDocumentPrepareContinueSource'
 attrs='@no_raise @c_result("owned_view") @factory(4,0,rollbackDocumentPrepare) @c_array_group(2,1,3,"uint64_t",4) @c_array_record(2,"xui_doc_source_patch_t","iSize","uint32_t") @c_array_field(2,"iStart",0,"value") @c_array_field(2,"iEnd",1,"value") @c_array_span_field(2,"sText","iTextBytes",2,"const char*","uint64_t")'
 classes['Document'].append(' extern "'+name+'" '+attrs+' func continueSource'+suffix+'(DocumentPrepare previous,array<uint64> starts,array<uint64> ends,array<'+kind+'> text,int size=-1) as DocumentPrepare')
mapped['xuiDocumentPrepareSource']='Document.prepareSource / Document.prepareSourceBytes';reasons.pop('xuiDocumentPrepareSource',None)
mapped['xuiDocumentPrepareContinueSource']='Document.continueSource / Document.continueSourceBytes';reasons.pop('xuiDocumentPrepareContinueSource',None)
# The streaming descriptor is packed, so callers never guess native sizeof.
classes['Document']=[line for line in classes['Document'] if 'extern "xuiDocumentPrepareStreamSource"' not in line]
classes['Document'].append(' extern "xuiDocumentPrepareStreamSource" @no_raise @c_result("owned_view") @factory(6,0,rollbackDocumentPrepare) @c_pack(2,"xui_doc_txn_desc_t",1,4) @c_pack_size(2,"iSize","uint32_t") @c_pack_field(2,"iDomain",1,"value") @c_pack_field(2,"iBaseRevision",2,"value") @c_pack_field(2,"iOrigin",3,"value") @c_pack_field(2,"iGroup",4,"value") @c_span(5,"const char*","size_t") func prepareStreamSource(DocumentPrepare? previous,c_uint domain=2,uint64 baseRevision=0,uint64 origin=0,uint64 group=0,string utf8="",c_int finalChunk=1) as DocumentPrepare')
classes['Document'].append(' // Continuation inherits the first descriptor. Native requires NULL here.')
classes['Document'].append(' extern "xuiDocumentPrepareStreamSource" @no_raise @c_result("owned_view") @factory(6,0,rollbackDocumentPrepare) @c_null(2,"const xui_doc_txn_desc_t*") @c_span(1,"const char*","size_t") func continueStreamSource(DocumentPrepare? previous,string utf8="",c_int finalChunk=1) as DocumentPrepare')

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
# Borrowed strings are copied before the native input scope ends. Keep scalar
# projections intact and offer explicitly named text companions.
for nativeName,cl,mn,ct,params,fields in [
 ('xuiDocumentSnapshotGetNode','DocumentSnapshot','nodeTextInfo','xui_doc_node_info_t','uint64 node',['sResource','sInfo','sTitle','sLinkTarget','sLinkTitle']),
 ('xuiDocumentTxnGetNode','DocumentTransaction','nodeTextInfo','xui_doc_node_info_t','uint64 node',['sResource','sInfo','sTitle','sLinkTarget','sLinkTitle']),
 ('xuiDocumentSnapshotQueryLink','DocumentSnapshot','linkInfo','xui_doc_link_info_t','DocumentRange range',['bLinked','bMixed','sUri','sTitle'])]:
 attrs=['@no_raise','@c_output_record(2,"'+ct+'","borrowed")','@c_output_size(2,"iSize")','@c_status(0)']
 if 'DocumentRange' in params:attrs+=['@c_ref(0)']
 attrs+=['@c_output_field('+str(i)+',"'+field+'"'+(',"cstring"' if field.startswith('s') else '')+')' for i,field in enumerate(fields)]
 returns=['string?' if field.startswith('s') else 'c_int' for field in fields]
 classes[cl].append(' extern "'+nativeName+'" '+' '.join(attrs)+' func '+mn+'('+params+') as ('+','.join(returns)+')')
 mapped[nativeName]+=' / '+cl+'.'+mn
classes['DocumentChangeSet'].append(' extern "xuiDocumentChangeSetGetOperations" @no_raise @c_owned_sequence(1,2,0,"size_t",0) @c_retain(retainDocumentChangeSet,"zero",rollbackDocumentChangeSet) func operations() as NativeSequence<DocumentOperation>')
functions.append('extern "xuiDocumentChangeSetAddRef" @header("xge3_runtime.h") @no_raise func retainDocumentChangeSet(DocumentChangeSet value) as c_int')
mapped['xuiDocumentChangeSetGetOperations']='DocumentChangeSet.operations';reasons.pop('xuiDocumentChangeSetGetOperations',None)
for nativeName,cl,methodName,record in [
 ('xuiDocumentMatchesGetRanges','DocumentMatches','ranges','DocumentRange'),
 ('xuiDocumentMarkdownLossesGetEntries','DocumentMarkdownLosses','entries','DocumentMarkdownLoss')]:
 prefix=nativeName[:nativeName.rindex('Get')]
 classes[cl].append(' extern "'+nativeName+'" @no_raise @c_owned_sequence(1,2,0,"size_t",0) @c_retain(retain'+cl+',"zero",rollback'+cl+') func '+methodName+'() as NativeSequence<'+record+'>')
 functions.append('extern "'+prefix+'AddRef" @header("xge3_runtime.h") @no_raise func retain'+cl+'('+cl+' value) as c_int')
 mapped[nativeName]=cl+'.'+methodName
 reasons.pop(nativeName,None)
find='xuiDocumentSnapshotFindOwned'
classes['DocumentSnapshot']=[line for line in classes['DocumentSnapshot'] if 'extern "'+find+'"' not in line]
findAttrs='@no_raise @c_span(1,"const char*","uint64_t") @c_result("owned_view") @factory(6,0,rollbackDocumentMatches)'
classes['DocumentSnapshot'].append(' extern "'+find+'" '+findAttrs+' @c_null(5,"const xui_doc_range_t*") func find(c_uint domain,string pattern,c_uint flags=0) as DocumentMatches')
classes['DocumentSnapshot'].append(' extern "'+find+'" '+findAttrs+' @c_ref(3) func findIn(c_uint domain,string pattern,c_uint flags,DocumentRange scope) as DocumentMatches')
mapped[find]='DocumentSnapshot.find / DocumentSnapshot.findIn'
reasons.pop(find,None)
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

for native_name, class_name in [
    ('xuiDocumentSnapshotGetNodeAttributes', 'DocumentSnapshot'),
    ('xuiDocumentTxnGetNodeAttributes', 'DocumentTransaction'),
]:
    classes[class_name].append(
        ' extern "' + native_name + '" @no_raise '
        '@c_output_record(2,"xui_doc_node_attributes_view_t","borrowed") @c_status(0) '
        '@c_output_field(0,"tValues") @c_output_field(1,"sFontFamily","cstring") '
        '@c_output_field(2,"sLanguage","cstring") '
        'func nodeAttributes(uint64 node) as (DocumentNodeAttributes,string,string?)'
    )
    mapped[native_name] = class_name + '.nodeAttributes'
    reasons.pop(native_name, None)

def node_attribute_pack(slot, first):
    return (
        f'@c_pack({slot},"xui_doc_node_attributes_view_t",{first},3) '
        f'@c_pack_field({slot},"tValues",{first},"value") '
        f'@c_pack_field({slot},"sFontFamily",{first+1},"text_data") '
        f'@c_pack_field({slot},"iFontFamilyBytes",{first+1},"text_size","size_t") '
        f'@c_pack_field({slot},"sLanguage",{first+2},"c_string")'
    )

classes['DocumentTransaction'].append(
    ' extern "xuiDocumentTxnSetNodeAttributes" @no_raise ' + node_attribute_pack(2, 1) +
    ' @c_result("status",0) func setNodeAttributes(uint64 node,DocumentNodeAttributes attributes,'
    'string fontFamily="",string? language=null)'
)
mapped['xuiDocumentTxnSetNodeAttributes'] = 'DocumentTransaction.setNodeAttributes'
reasons.pop('xuiDocumentTxnSetNodeAttributes', None)

classes['DocumentTransaction'].append(
    ' extern "xuiDocumentTxnInsertNodeWithAttributes" @no_raise ' + node_attribute_pack(3, 2) +
    ' @c_pack(4,"xui_doc_node_desc_t",5,10) @c_pack_size(4,"iSize","uint32_t")'
    ' @c_pack_field(4,"iKind",5,"value") @c_pack_field(4,"sText",6,"text_data")'
    ' @c_pack_field(4,"iTextBytes",6,"text_size","size_t")'
    ' @c_pack_field(4,"sResource",7,"c_string") @c_pack_field(4,"sInfo",8,"c_string")'
    ' @c_pack_field(4,"sTitle",9,"c_string") @c_pack_field(4,"sLinkTarget",10,"c_string")'
    ' @c_pack_field(4,"sLinkTitle",11,"c_string")'
    ' @c_span(12,"const void*","uint64_t") @c_pack_field(4,"pExtensionPayload",12,"span_data")'
    ' @c_pack_field(4,"iExtensionPayloadBytes",12,"span_count")'
    ' @c_pack_field(4,"iExtensionVersion",13,"value") @c_pack_field(4,"bExtensionRequired",14,"value")'
    ' @c_output(5,0) func insertNodeWithAttributes(uint64 parent,uint64 childIndex,'
    'DocumentNodeAttributes attributes,string fontFamily,string? language,c_uint kind,'
    'string text="",string? resource=null,string? info=null,string? title=null,'
    'string? linkTarget=null,string? linkTitle=null,bytes extensionPayload=b"",'
    'c_uint extensionVersion=0,c_int extensionRequired=0) as uint64'
)
mapped['xuiDocumentTxnInsertNodeWithAttributes'] = 'DocumentTransaction.insertNodeWithAttributes'
reasons.pop('xuiDocumentTxnInsertNodeWithAttributes', None)

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
 for kind in (('bytes','Buffer')if nativeName=='xuiDocumentFragmentDeserializeOwned'else('string','bytes','Buffer')):
  functions.append('export extern "'+nativeName+'" @header("xge3_runtime.h") @no_raise @c_span(0,"const char*","uint64_t",1) @c_result("owned_view") @factory(2,0,rollbackDocumentFragment) func '+logical+'('+kind+' data,int size=-1) as DocumentFragment')
 mapped[nativeName]=logical;reasons.pop(nativeName,None)
# Each descriptor is stack-packed; allocator callbacks remain NULL.
fields=['iProfile','iMarkdownDialect','iHistoryLimit','iHistoryMaxBytes','bDisableHistory','iMaxTextBytes','iMaxNodes','iMaxSyntaxRecords']
config='c_uint profile=1,c_uint markdownDialect=0,c_uint historyLimit=0,uint64 historyMaxBytes=0,c_int disableHistory=0,uint64 maxTextBytes=0,uint64 maxNodes=0,uint64 maxSyntaxRecords=0'
for nativeName,logical,first,packSlot,outSlot,baseParameters in [('xuiDocumentOpenFileOwned','openDocument',2,2,4,'string path,c_uint format'),('xuiDocumentDeserializeOwned','deserializeDocument',2,0,3,'')]:
 attrs=['@no_raise','@c_result("owned_view")','@factory('+str(outSlot)+',0,rollbackDocument)']
 attrs+=['@c_cstr(0)'] if packSlot==2 else ['@c_span(0,"const char*","uint64_t",1)']
 attrs+=['@c_pack('+str(packSlot)+',"xui_doc_desc_t",'+str(first)+',8)','@c_pack_size('+str(packSlot)+',"iSize","uint32_t")']
 attrs+=['@c_pack_field('+str(packSlot)+',"'+field+'",'+str(first+i)+',"value")' for i,field in enumerate(fields)]
 for kind in (('',)if packSlot==2 else('string','bytes','Buffer')):
  parameters=baseParameters+','+config+',uint64 maxFileBytes=0'if packSlot==2 else kind+' data,int size=-1,'+config
  functions.append('export extern "'+nativeName+'" @header("xge3_runtime.h") '+' '.join(attrs)+' func '+logical+'('+parameters+') as Document')
 mapped[nativeName]=logical;reasons.pop(nativeName,None)
for n in ('xui_document','xui_document_change_set'):

 cl,tag,prefix,policy=models[n];cb='Callback'+cl
 lines+=['export extern "struct '+tag+'*" class '+cb+' @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "'+prefix+'ShareOwned" @no_raise @c_result("owned_view") @factory(1,0,rollback'+cl+') func share() as '+cl,*[line for line in classes[cl] if any(('func '+mn+'(') in line for mn in (('identity','revision','profile','markdownDialect') if cl=='Document' else ('info',)))],'end','']
lines+=['export extern "const xui_doc_source_binding_t*" class CallbackDocumentSourceBinding @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "value" @c_deref get value() as DocumentSourceBinding','end','export typedef DocumentSourceBindingProc = func(uint64,CallbackDocumentSourceBinding) as c_int','']
lines+=['export typedef DocumentChangeProc = func(CallbackDocument,CallbackDocumentChangeSet) as c_int','',*functions,'']
src=r/'lib/bindings/xui/src'
for name,text in [('document_records.xl','\n'.join(recordLines)+'\n'),('document.xl','\n'.join(lines))]:
 if args.check:assert (src/name).read_text(encoding='utf-8')==text,'generated declarations differ: '+name
 else:(src/name).write_text(text,encoding='utf-8',newline='\n')
report=dict(scope='Default-provenance Model A direct declaration coverage; declarations are not ABI/runtime qualification',authority=h['header'],authoritySha256=h['sha256'],roots={k:dict(logical=v[0],cName='struct '+v[1]+'*',policy='xl6Xui3'+v[3]+'Policy') for k,v in models.items()},records=records,pointerBearingRecords=list(pending),entries=[dict(native=f['name'],declaration=f['declaration'],line=f['line'],logical=mapped.get(f['name']),state='declared-needs-ABI' if f['name'] in mapped else 'explicit-C-interoperability',reason=reasons.get(f['name'])) for f in h['functions']],counts=dict(publicFunctions=len(h['functions']),declared=len(mapped),ownedRoots=len(models),pointerFreeRecords=len(records),constants=len(constantRows)))

replacements={
 'xuiDocumentSnapshotAnalyzeMarkdownConversion':'Legacy caller-owned result buffer. DocumentSnapshot.collectMarkdownLosses returns all entries in an immutable owned report.',
 'xuiDocumentSnapshotFind':'Legacy caller-owned match buffer. DocumentSnapshot.find and findIn use literal search when flags are zero.',
 'xuiDocumentSnapshotFindEx':'Legacy caller-owned match buffer. DocumentSnapshot.find and findIn support the same flags and domain.',
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
  entry['reason']='Owned resident-XRT export; ordinary paired byte/text declarations consume exact length. FreeBuffer remains internal to language cleanup.'if entry['native']!='xuiDocumentFreeBuffer'else'Resident-XRT release is handled by language value cleanup; no public raw output pointer.'
 if name in replacements:entry['reason']=replacements[name]
report['fields']={
 'xui_doc_node_info_t':dict(projectedPOD=['iKind','iId','iParentId','iChildCount','iTextBytes','iSourceStart','iSourceEnd','bSourceExact','iSyntaxStart','iSyntaxEnd','iSourceSegmentCount','iExtensionPayloadBytes','iExtensionVersion','bExtensionRequired'],projectedText=['sResource','sInfo','sTitle','sLinkTarget','sLinkTitle'],nodeAttributes='The dedicated nodeAttributes method returns the numerical attributes plus owned font-family and optional language strings.',safeTextAlternative='copyText/readText and copySource/readSource write into scoped Buffer storage.'),
 'xui_doc_change_info_t':dict(projectedPOD=['iFlags','iBeforeRevision','iAfterRevision','iOrigin','iInputGeneration','iOperationCount','bUndo','iDomain'],readonlyOperations='DocumentChangeSet.operations returns a NativeSequence that owns a reference to the change set.',notProjected=dict(pOperations='The raw field stays private. The dedicated operations getter returns the same contiguous POD array.')),
 'xui_doc_link_info_t':dict(projectedPOD=['bLinked','bMixed'],projectedText=['sUri','sTitle'],notProjected={}),
 'xui_doc_attributes_t':dict(input='setNodeAttributes uses DocumentNodeAttributes, a counted font family and nullable language. setAttributesColumns remains the compatibility form with an empty font family.'),
 'xui_doc_node_attributes_view_t':dict(projectedPOD=['tValues'],projectedText=['sFontFamily','sLanguage'],input='The exact text/byte count of the font family is packed without C-string truncation. Native applies its UTF-8, length and schema limits.'),
 'xui_doc_node_desc_t':dict(input='insertNode uses default attributes. insertNodeWithAttributes also packs the separate attributes view and the exact bytes, version and required flag of an optional extension payload. Native admits and copies all inputs in one insertion.'),
 'xui_doc_image_desc_t':dict(input='insertImage/updateImage pack all public fields with counted alt text and nullable NUL strings.'),
 'xui_doc_source_index_desc_t':dict(input='createSourceIndex packs default allocator settings and maxWorkingBytes.',notProjected=dict(onCancel='Callback nested inside a packed descriptor is not synthesized as a persistent raw callback.',pCancelUser='No user pointer exposed.')),
 'xui_doc_desc_t':dict(input='Owned constructors pack size and all eight ordinary settings.',notProjected=dict(onAlloc='Custom allocator callback lifetime is C-only.',onFree='Must be paired with allocator, outlives all handles.',pAllocatorUser='No raw userdata exposed.')),
 'xui_doc_source_patch_t':dict(input='prepareSource/continueSource and their Bytes variants project parallel start/end/text columns. Each temporary record receives exact sizeof, data pointer and actual byte length; readonly pins retain the original string/bytes backing for the synchronous call. Native copies accepted source text before return.',notProjected=dict(writableOutput='Input-only record projection; no writable language array or raw pointer is exposed.'))}
report['costs']=dict(nativeForwardingFunctions=0,descriptorPacks='Stack initialization and field assignments; counted inputs are pinned; NUL strings require validation/scans.',owningRoots='One resident control/code lease and real native owner per independent root; share/factories reserve before producer.',POD='Exact native values or stack output-record field copies; no arbitrary pointer projection.',buffers='Scope/pin/length checks; Native Copy performs its documented bytes copy; lengths remain unchanged.',callbacks='Native Owned capture ownership plus checked epoch admission; explicit share acquires a real separate owner.',overallZeroOverheadClaim=False)
report['externalTypeAuthority']=dict(header=ui['header'],sha256=ui['sha256'])
report['costs']['ownedExports']='Text adopts the actual resident-XRT allocation after one UTF-8 scan; bytes adds its ordinary owning value carrier. No payload copy or strlen.'
report['costs']['sourcePatchInputs']='One zero-initialized O(n) native record array plus column read pins and checked field writes. No text copy or NUL scan in the projection; native source processing retains its own documented copies. Default empty array arguments may allocate ordinary input containers.'
report['costs']['queryResults']='Search and loss collection build one native result per query. Geometric growth can copy its native backing while unpublished. NativeSequence takes a real reference and reads the final immutable backing directly without an array copy.'
report['costs']['nodeAttributes']='Native translates the exact POD fields and copies the counted font family to inline storage. Language query copies the font family and optional language within the native result scope. The original document transaction performs its usual validation, allocations and COW.'
previous=r/'docs/xge3-xui3-document-api-map.json'
if previous.is_file():
 old=json.loads(previous.read_text(encoding='utf-8'))
 if 'qualification'in old:report['previousQualification']=old['qualification']
 elif 'previousQualification'in old:report['previousQualification']=old['previousQualification']
receipt=r/'dev/release-materials/provenance/xui-document-exports-source-20261112.json'
if receipt.is_file():
 report['ownedExportQualification']=dict(index=str(receipt.relative_to(r)).replace('\\','/'),sha256=hashlib.sha256(receipt.read_bytes()).hexdigest(),scope='Only owned export additions, not every declared Document entry; complete O2/VFS and final delivery tracked separately.')

if not args.check:(r/'docs/xge3-xui3-document-api-map.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(report['counts']);print('records pending',list(pending))
