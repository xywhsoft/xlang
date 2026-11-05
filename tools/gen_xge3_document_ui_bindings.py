"""Generate direct Document UI declarations from one matched Native authority.

Only explicitly audited ownership/callback/descriptor contracts are added.
Every remaining C interface has an individual inventory reason.
"""
from pathlib import Path
import argparse,sys,re,json
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--native-root',type=Path,required=True);a=p.parse_args();root=a.root.resolve();native=a.native_root.resolve();sys.path.insert(0,str(root/'tools'));import check_xge3_bindings as inv
known=set();previous=[]
for n in ('xge.h','xui.h','xui_document.h'):
 h=inv.header_inventory(native/'include'/n,known);known.update(x['name'] for x in h['types']);previous+=h['types']
h=inv.header_inventory(native/'include/xui_document_ui.h',known);assert not h['parseErrors'];types={x['name']:x['type'] for x in previous+h['types']};records={}
for path in ('records.xl','document_records.xl'):
 records.update(re.findall(r'export extern "([^"]+)" struct (\w+)',(root/'lib/bindings/xui/src'/path).read_text()))
primitives=dict(inv.SCALARS,xui_doc_node_id='uint64');reserved=set(re.findall(r'\{"([A-Za-z_]\w*)", XL6_KW_',(root/'src/lexer/xl6_lexer_gen.c').read_text()))
def named(t):return ' '.join(t.get('names',[])) if t['kind']=='named' else None
def value(t,seen=()):
 n=named(t)
 if n in primitives:return primitives[n]
 if n in records:return records[n]
 if n in types and n not in seen and types[n]['kind'] not in ('struct','union'):return value(types[n],(*seen,n))
 if t['kind']=='array':return value(t['of'],seen)
 raise ValueError('not pointer-free POD: '+str(t))
lines=['// Pointer-free exact Document UI C records. Owning descriptors and callback outputs are excluded.'];pending={x['name']:x['type'] for x in h['types'] if x['type']['kind']=='struct' and x['type'].get('fields') is not None};newRecords={}
while pending:
 changed=False
 for name,t in list(pending.items()):
  try:
   fields=[]
   for f in t['fields']:
    ty=value(f['type']);suffix=''
    if f['type']['kind']=='array':
     extent=f['type']['extent'];assert str(extent).isdigit();suffix='['+str(extent)+']'
    fields.append(' '+ty+' '+f['name']+suffix)
  except (ValueError,AssertionError,KeyError):continue
  logical='Document'+''.join(x.title() for x in name.removeprefix('xui_doc_').removesuffix('_t').split('_'));records[name]=logical;newRecords[name]=logical;lines+=['export extern "'+name+'" struct '+logical+' @header("xge3_runtime.h")',*fields,'end',''];del pending[name];changed=True
 if not changed:break
constants=[];constantValues={}
for en in h['enums']:
 v=-1
 for e in en['type']['values']:
  n=e['name'];v=inv.integer(e['expression'],constantValues) if e['expression'] else v+1;constantValues[n]=v;constants.append((n,v));lines.append('export const c_uint '+n+' = '+str(v))
(root/'lib/bindings/xui/src/document_ui_records.xl').write_text('\n'.join(lines).rstrip()+'\n')
classes={n:[] for n in ('DocumentRenderer','DocumentView','DocumentEditor','DocumentWebProvider','DocumentHtmlInteraction','DocumentImageRequest','MessageList')};mapped={};reasons={};globalFunctions=[]
handles={'xui_context':'Context','xui_widget':'Widget','xui_document':'Document','xui_document_snapshot':'DocumentSnapshot','xui_document_change_set':'DocumentChangeSet','xui_document_prepare':'DocumentPrepare','xui_proxy':'Proxy','xui_draw_context':'DrawContext','xui_font':'Font'}
nullable={'xuiDocumentRendererSetSnapshot':{'changes'},'xuiDocumentRendererSetSourceInput':{'prepare'}}
intValues={'xuiDocumentEditorCanExecute','xuiDocumentEditorGetReadOnly','xuiDocumentEditorIsComposing'}
def decl(name,mn,params,attrs=(),returns=None):return ' extern "'+name+'" @no_raise '+(' '.join(attrs)+' ' if attrs else '')+'func '+mn+'('+params+')'+(' as '+returns if returns else '')
def add(cl,name,mn,params,attrs=(),returns=None):classes[cl].append(decl(name,mn,params,attrs,returns));mapped.setdefault(name,[]).append(cl+'.'+mn);reasons.pop(name,None)
def status(cl,name,mn,params='',attrs=()):add(cl,name,mn,params,[*attrs,'@c_result("status",0)'])
def owner(name):
 for prefix,cl in [('xuiDocumentRenderer','DocumentRenderer'),('xuiDocumentView','DocumentView'),('xuiDocumentEditor','DocumentEditor'),('xuiDocumentWebProvider','DocumentWebProvider'),('xuiDocumentHtmlInteraction','DocumentHtmlInteraction'),('xuiDocumentImageResource','DocumentImageRequest'),('xuiMessageList','MessageList')]:
  if name.startswith(prefix):return cl,prefix
 return None,None
for f in h['functions']:
 name=f['name'];cl,prefix=owner(name);ps=f['type']['parameters']
 if not cl:reasons[name]='Explicit callback-output helper or legacy void-userdata provider adapter';continue
 if any(x in name for x in ('Create','LifetimeWatch','TraceOwnership','OwnershipBusy','Release','RequestClose','GetContext','GetType')) or name.endswith('Owned') or name.endswith('SetOpenLink') or name.endswith('SetInvalidate'):
  reasons[name]='Native lifetime/registration infrastructure; explicit typed declaration follows where safe';continue
 if name=='xuiDocumentRendererGetRangeRects':reasons[name]='Caller rectangle output array/capacity needs a checked typed writable output-array contract; never one record plus arbitrary capacity';continue
 if name in ('xuiDocumentHtmlInteractionGetWindow','xuiDocumentHtmlInteractionGetWebView','xuiDocumentEditorGetMenuWidget','xuiDocumentEditorGetFindWindow'):
  add(cl,name,name[len(prefix):][3:4].lower()+name[len(prefix):][4:],'',['@c_result("owner_view",0)'],'Widget?');continue
 if name=='xuiDocumentImageResourcePoll':add(cl,name,'poll','',['@c_null(1,"xui_resource*")'],'c_int');continue
 if name=='xuiDocumentImageResourceCancel':status(cl,name,'cancel');continue
 if name in ('xuiDocumentImageResourceLoadMemory','xuiDocumentImageResourceLoadFile','xuiDocumentImageResourceGetAsyncStats'):
  reasons[name]='Context global operation is added explicitly below';continue
 if not ps or named(ps[0]['type']) not in ('xui_widget','xui_document_renderer','xui_doc_web_provider','xui_doc_html_interaction'):reasons[name]='No audited managed receiver';continue
 if name in ('xuiDocumentViewGetDocument','xuiMessageListGetNodeDocument'):
  pars='string message_id' if cl=='MessageList' else '';attrs=['@c_result("retained_view")','@c_retain(retainDocumentUi,"zero",rollbackDocument)']+(['@c_cstr(0)'] if pars else []);add(cl,name,'nodeDocument' if pars else 'document',pars,attrs,'Document?');continue
 attrs=[];params=[];outs=[];outTypes=[];sizes=[];skip=set()
 try:
  for physical,param in enumerate(ps[1:],1):
   if physical in skip:continue
   t=param['type'];n=named(t);pn=param['name'];pn=pn+'Value' if pn in reserved else pn
   if n in handles:params.append(handles[n]+('?' if param['name'] in nullable.get(name,set()) else '')+' '+pn);continue
   if t['kind']=='pointer':
    target=t['to'];tn=named(target);const='const' in target.get('qualifiers',[])
    if const and tn=='char' and physical+1<len(ps) and named(ps[physical+1]['type'])=='uint64_t':attrs.append('@c_span('+str(len(params))+',"const char*","size_t")');params.append('string '+pn);skip.add(physical+1);continue
    if const and tn=='char':attrs.append('@c_cstr('+str(len(params))+')');params.append('string '+pn);continue
    if const and tn in records:attrs.append('@c_ref('+str(len(params))+')');params.append(records[tn]+' '+pn);continue
    if not const and tn in records:outs.append(physical);outTypes.append(records[tn]);sizes.extend([physical] if types[tn]['fields'] and types[tn]['fields'][0]['name']=='iSize' else []);continue
    if not const and tn in primitives and tn!='void':outs.append(physical);outTypes.append(primitives[tn]);continue
    raise ValueError('Pointer-bearing descriptor or untracked pointer output')
   params.append(value(t)+' '+pn)
  returns=value(f['type']['returns'])
  if outs:
   assert returns=='c_int';attrs+=['@c_output('+str(outs[0])+',0)'] if len(outs)==1 else ['@c_outputs('+','.join(map(str,outs))+')','@c_status(0)'];attrs+=['@c_output_size('+str(x)+',"iSize")' for x in sizes];returns=outTypes[0] if len(outs)==1 else '('+','.join(outTypes)+')'
  elif returns=='c_int' and name not in intValues:attrs.append('@c_result("status",0)');returns=None
  mn=name[len(prefix):];mn=mn[3:] if mn.startswith('Get') else mn;mn=mn[:1].lower()+mn[1:];add(cl,name,mn,','.join(params),attrs,returns)
 except (ValueError,AssertionError,KeyError) as error:reasons[name]=str(error)

# Epoch-only views, with explicit copying or native ownership acquisition to escape.
prefixLines=['// Direct native Document UI methods. Resource pins, callback epochs and checked cleanup remain real costs.',
'export extern "const xstrview*" class CallbackDocumentText @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "Size" @c_field get length() as c_size',' extern "Data" @c_field @c_length("Size","size_t") @c_result("text_view") get text() as string','end','',
'export extern "float*" class CallbackDocumentFloat @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "value" @c_deref get value() as c_float',' extern "value" @c_deref let value(c_float number)','end','',
'export extern "struct xui_doc_snapshot_t*" class CallbackDocumentSnapshot @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "xuiDocumentSnapshotShareOwned" @no_raise @c_result("owned_view") @factory(1,0,rollbackDocumentSnapshot) func share() as DocumentSnapshot','end','',
'export extern "struct xui_document_renderer_t*" class CallbackDocumentRenderer @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "xuiDocumentRendererRequestClose" @no_raise @c_result("status",0) func requestClose()','end','',
'export extern "xui_doc_font_output_t*" class CallbackDocumentFontOutput @header("xge3_runtime.h") @lifetime("callback")',' native point handle',' extern "xuiDocumentFontOutputSetFont" @no_raise @c_result("status",0) func setFont(Font font)',' extern "xuiDocumentFontOutputGetFont" @no_raise @c_const(0) @c_result("retained_view") @c_retain(retainFont,"zero",rollbackFont) func font() as Font?',' extern "xuiDocumentFontOutputDisposeChecked" @no_raise @c_result("status",0) func clear()','end','',
'export typedef DocumentFontResolverProc = func(CallbackDocumentRenderer,CString,c_uint,c_float,CallbackDocumentFontOutput) as c_int',
'export typedef DocumentObjectMeasureProc = func(CallbackDocumentRenderer,CallbackDocumentSnapshot,uint64,c_float,c_float,CallbackVec2,CallbackDocumentFloat) as c_int',
'export typedef DocumentObjectDrawProc = func(CallbackDocumentRenderer,CallbackDocumentSnapshot,uint64,CallbackDrawContext,Rect) as c_int',
'export typedef DocumentActivateProc = func(Widget,uint64,CallbackDocumentText)',
'export typedef DocumentTaskToggleProc = func(Widget,uint64,c_int)',
'export typedef DocumentEditorErrorProc = func(Widget,c_int)',
'export typedef DocumentWebInvalidateProc = func()',
'export typedef DocumentOpenLinkProc = func(CallbackDocumentText)','']
for name,mn,ty in [('FontResolver','setFontResolver','DocumentFontResolverProc'),('ObjectMeasure','setObjectMeasure','DocumentObjectMeasureProc'),('ObjectDraw','setObjectDraw','DocumentObjectDrawProc')]:status('DocumentRenderer','xuiDocumentRendererSet'+name+'Owned',mn,ty+'? callback',['@c_owned_callback(0,0,"xui_callback_owner_t",-16)'])
status('DocumentView','xuiDocumentViewSetActivateOwned','setActivate','DocumentActivateProc? callback',['@c_owned_callback(0,0,"xui_callback_owner_t",0)'])
status('DocumentEditor','xuiDocumentEditorSetErrorOwned','setError','DocumentEditorErrorProc? callback',['@c_owned_callback(0,0,"xui_callback_owner_t",0)'])
status('DocumentHtmlInteraction','xuiDocumentHtmlInteractionSetOpenLinkOwned','setOpenLink','DocumentOpenLinkProc? callback',['@c_owned_callback(0,0,"xui_callback_owner_t",0)'])
status('DocumentWebProvider','xuiDocumentWebProviderSetInvalidateOwned','setInvalidate','DocumentWebInvalidateProc? callback',['@c_owned_callback(0,0,"xui_callback_owner_t",0)'])
for suffix,mn,ty in [('Activate','setNodeDocumentActivate','DocumentActivateProc'),('TaskToggle','setNodeDocumentTaskToggle','DocumentTaskToggleProc')]:status('MessageList','xuiMessageListSetNodeDocument'+suffix+'Owned',mn,'string messageId,'+ty+'? callback',['@c_cstr(0)','@c_owned_callback(1,0,"xui_callback_owner_t",0)'])

# Stack descriptors keep all legacy callback/userdata slots NULL. Font handles
# are acquired/pinned as real managed inputs, not copied language pointers.
rendererFields=[('fZoom','c_float zoom=1','value'),('fParagraphGap','c_float paragraphGap=0','value'),('fLineGap','c_float lineGap=0','value'),('fIndent','c_float indent=0','value'),('tFonts.normal','Font? normal=null','view_raw'),('tFonts.bold','Font? bold=null','view_raw'),('tFonts.italic','Font? italic=null','view_raw'),('tFonts.boldItalic','Font? boldItalic=null','view_raw'),('tFonts.monospace','Font? monospace=null','view_raw'),('iTextColor','c_uint textColor=0','value'),('iBorderColor','c_uint borderColor=0','value'),('iCodeBackground','c_uint codeBackground=0','value'),('iHighlightColor','c_uint highlightColor=0','value'),('iLinkColor','c_uint linkColor=0','value'),('iQuoteBorderColor','c_uint quoteBorderColor=0','value'),('iRuleColor','c_uint ruleColor=0','value'),('iParagraphBackgroundColor','c_uint paragraphBackgroundColor=0','value'),('iTableBorderColor','c_uint tableBorderColor=0','value'),('iTableHeaderColor','c_uint tableHeaderColor=0','value'),('iTableCellColor','c_uint tableCellColor=0','value'),('iImagePlaceholderColor','c_uint imagePlaceholderColor=0','value'),('iImageBorderColor','c_uint imageBorderColor=0','value'),('iImageTextColor','c_uint imageTextColor=0','value'),('iLayoutCacheBudgetBytes','uint64 layoutCacheBudgetBytes=0','value'),('bSourceLineHeightMayVary','c_uint sourceLineHeightMayVary=0','value')]
def pack(slot,ct,first,fields,nested=()):
 attrs=['@c_pack('+str(slot)+',"'+ct+'",'+str(first)+','+str(len(fields))+')','@c_pack_size('+str(slot)+',"iSize","uint32_t")']
 attrs+=['@c_pack_record('+str(slot)+',"'+path+'","'+ty+'")' for path,ty in nested]
 attrs+=['@c_pack_field('+str(slot)+',"'+field+'",'+str(first+i)+',"'+kind+'"'+(',"size_t"' if kind=='text_size' else '')+')' for i,(field,_,kind) in enumerate(fields)]
 return attrs
def factory(name,mn,params,attrs,cl,slot,ownerIndex=None):
 attrs=['@c_result("owned_view")' if ownerIndex is None else '@c_result("owner_view",'+str(ownerIndex)+')','@factory('+str(slot)+',0,rollback'+cl+')',*attrs];globalFunctions.append('export'+decl(name,mn,params,attrs,cl)[0:]);mapped[name]=[mn];reasons.pop(name,None)
factory('xuiDocumentRendererCreate','createDocumentRenderer','Context context,'+','.join(x[1] for x in rendererFields),pack(1,'xui_doc_renderer_desc_t',1,rendererFields,[('tFonts','xui_doc_font_set_t')]),'DocumentRenderer',2)
for cl,ct,pref in [('DocumentView','xui_doc_view_desc_t',''),('DocumentEditor','xui_doc_editor_desc_t','tView.')]:
 fields=[(pref+'pDocument','Document document','view_raw')]+[(pref+'tRenderer.'+f,p,k) for f,p,k in rendererFields]+[(pref+'bAutoHeight','c_int autoHeight=0','value'),(pref+'bDisableSelection','c_int disableSelection=0','value'),(pref+'iSelectionColor','c_uint selectionColor=0','value'),(pref+'iBackgroundColor','c_uint backgroundColor=0','value')]
 if cl=='DocumentEditor':fields += [('iMode','c_uint mode=1','value'),('bReadOnly','c_int readOnly=0','value'),('iCaretColor','c_uint caretColor=0','value'),('iAsyncSourceThresholdBytes','uint64 asyncSourceThresholdBytes=0','value'),('iUndoGroupTimeoutMs','c_uint undoGroupTimeoutMs=0','value')]
 nested=([('tView','xui_doc_view_desc_t')] if pref else [])+[(pref+'tRenderer','xui_doc_renderer_desc_t'),(pref+'tRenderer.tFonts','xui_doc_font_set_t')];attrs=pack(1,ct,1,fields,nested)+['@c_pack_size(1,"'+pref+'tRenderer.iSize","uint32_t","'+pref+'tRenderer")']
 if pref:attrs+=['@c_pack_size(1,"tView.iSize","uint32_t","tView")']
 factory('xui'+cl+'Create','create'+cl,'Context context,'+','.join(x[1] for x in fields),attrs,cl,2,0)
fields=[('pDocument','Document document','view_raw')]+[('tRenderer.'+f,p,k) for f,p,k in rendererFields]+[('iSelectionColor','c_uint selectionColor=0','value')]
status('MessageList','xuiMessageListSetNodeDocument','setNodeDocument','string messageId,'+','.join(x[1] for x in fields),['@c_cstr(0)',*pack(2,'xui_message_document_desc_t',1,fields,[('tRenderer','xui_doc_renderer_desc_t'),('tRenderer.tFonts','xui_doc_font_set_t')]),'@c_pack_size(2,"tRenderer.iSize","uint32_t","tRenderer")'])
nodeFields=[('sId','string id','c_string'),('sText','string text=""','c_string'),('sSender','string? sender=null','c_string'),('sTime','string? time=null','c_string'),('sParentId','string? parentId=null','c_string'),('sTitle','string? title=null','c_string'),('iType','c_int type=0','value'),('iFlags','c_int flags=0','value'),('iAuxiliaryKind','c_int auxiliaryKind=0','value')]
# __xuiMessageCopyNode validates then copies every text before publication;
# pUser stays zero. This makes the Document attachment usable without a raw node.
status('MessageList','xuiMessageListAddNode','addNode',','.join(x[1] for x in nodeFields),pack(1,'xui_message_node_t',0,nodeFields))
for name,mn,ct,fields in [('xuiDocumentWebProviderCreate','createDocumentWebProvider','xui_doc_web_provider_desc_t',[('pContext','Context context','view_raw'),('pParent','Widget parent','view_raw'),('sAssetsFolder','string assetsFolder','c_string'),('sUserDataFolder','string? userDataFolder=null','c_string'),('iTimeoutMs','c_uint timeoutMs=0','value')]),('xuiDocumentHtmlInteractionCreate','createDocumentHtmlInteraction','xui_doc_html_interaction_desc_t',[('pContext','Context context','view_raw'),('pDocument','Document document','view_raw'),('iNodeId','uint64 node','value'),('tBounds','Rect bounds','value'),('sTitle','string? title=null','c_string')])]:factory(name,mn,','.join(x[1] for x in fields),pack(0,ct,0,fields), 'DocumentWebProvider' if 'WebProvider' in name else 'DocumentHtmlInteraction',1,0)

limits=[('iMaxEncodedBytes','uint64 maxEncodedBytes=0','value'),('iMaxPixels','uint64 maxPixels=0','value')]
for memory in (False,True):
 for asynchronous in (False,True):
  name='xuiDocumentImageResourceLoad'+('Memory' if memory else 'File')+('Async' if asynchronous else '');mn='loadDocumentImage'+('Memory' if memory else 'File')+('Async' if asynchronous else '');slot=4 if memory else 3;outSlot=slot+1;params='Context context,string resourceName,'+('bytes encoded' if memory else 'string authorizedPath')+','+','.join(x[1] for x in limits);attrs=['@c_cstr(1)',*pack(slot,'xui_doc_image_load_limits_t',3,limits)]
  attrs+=['@c_span(2,"const void*","size_t")'] if memory else ['@c_cstr(2)']
  if asynchronous:factory(name,mn,params,attrs,'DocumentImageRequest',outSlot,0)
  else:
   attrs+=['@c_null('+str(outSlot)+',"xui_resource*")','@c_result("status",0)'];globalFunctions.append('export'+decl(name,mn,params,attrs));mapped[name]=[mn];reasons.pop(name,None)
name='xuiDocumentImageResourceGetAsyncStats';globalFunctions.append('export'+decl(name,'documentImageAsyncStats','Context context',['@c_output(1,0)','@c_output_size(1,"iSize")'],'DocumentImageAsyncStats'));mapped[name]=['documentImageAsyncStats'];reasons.pop(name,None)

# Flat image descriptor packing preserves Native copies and caller-provided
# byte counts; the legacy libc output allocator is never adopted here.
for name,mn,first in [('xuiDocumentEditorInsertImage','insertImage',0),('xuiDocumentEditorUpdateImage','updateImage',1)]:
 fs=[('sResource','string resource','c_string'),('sAlt','string alt=""','text_data'),('iAltBytes',None,'text_size'),('sTitle','string? title=null','c_string'),('sLinkTarget','string? linkTarget=null','c_string'),('sLinkTitle','string? linkTitle=null','c_string'),('fWidth','c_float width=0','value'),('fHeight','c_float height=0','value')];logical=[x for x in fs if x[1]];attrs=pack(first+1,'xui_doc_image_desc_t',first,logical);attrs=[s.replace('"sAlt",'+str(first+1)+',"text_data")','"sAlt",'+str(first+1)+',"text_data")') for s in attrs];attrs+=['@c_pack_field('+str(first+1)+',"iAltBytes",'+str(first+1)+',"text_size","size_t")'];status('DocumentEditor',name,mn,('uint64 imageId,' if first else '')+','.join(x[1] for x in logical),attrs)

output=prefixLines[:]
for cl in ('DocumentRenderer','DocumentView','DocumentEditor','DocumentWebProvider','DocumentHtmlInteraction','DocumentImageRequest'):
 if cl=='DocumentRenderer':physical='struct xui_document_renderer_t*';attributes='@lifetime("owned") @policy("xl6Xui3DocumentRendererPolicy") @domain("xl6Xui3OwnershipDomainV1")';base='';close=' extern "xl6ForeignViewCloseV1" @no_raise @c_control(0) @c_result("bool_status") func close()';release='xuiDocumentRendererReleaseChecked'
 elif cl in ('DocumentView','DocumentEditor'):physical='xui_widget_t*';attributes='@lifetime("owner") @owner(Context) @policy("xl6Xui3WidgetPolicy") @watch("xui_lifetime_link_t")';base=' extends '+('Widget' if cl=='DocumentView' else 'DocumentView');close='';release='xuiWidgetDestroyChecked'
 else:
  physical={'DocumentWebProvider':'struct xui_doc_web_provider_t*','DocumentHtmlInteraction':'struct xui_doc_html_interaction_t*','DocumentImageRequest':'struct xui_doc_image_request_t*'}[cl];attributes='@lifetime("owner") @owner(Context) @policy("xl6Xui3'+cl+'Policy") @watch("xui_lifetime_link_t")';base='';release='xuiDocumentImageResourceReleaseChecked' if cl=='DocumentImageRequest' else 'xui'+cl+'ReleaseChecked';close=decl(release,'close','',['@c_result("status",0)'])
 output+=['export extern "'+physical+'" class '+cl+' @header("xge3_runtime.h") '+attributes+base,*([' native point handle'] if not base else []),*classes[cl],*([close] if close else []),'end',decl(release,'rollback'+cl,cl+' value',['@header("xge3_runtime.h")','@c_result("status",0)']).lstrip(),'']
output+=['extern "xuiDocumentAddRef" @header("xge3_runtime.h") @no_raise func retainDocumentUi(Document value) as c_int','',*[line.replace(' @no_raise ',' @header("xge3_runtime.h") @no_raise ',1) for line in globalFunctions]]
(root/'lib/bindings/xui/src/document_ui.xl').write_text('\n'.join(output).rstrip()+'\n')
for name,logical in [('xuiDocumentFontOutputSetFont','CallbackDocumentFontOutput.setFont'),('xuiDocumentFontOutputGetFont','CallbackDocumentFontOutput.font'),('xuiDocumentFontOutputDisposeChecked','CallbackDocumentFontOutput.clear'),('xuiDocumentRendererRequestClose','CallbackDocumentRenderer.requestClose')]:mapped[name]=[logical];reasons.pop(name,None)
message='\n// Document UI declarations generated from xui_document_ui.h.\n'+'\n'.join(classes['MessageList'])+'\n'
(root/'lib/bindings/xui/src/document_ui_message_methods.inc').write_text(message)
path=root/'lib/bindings/xui/src/managed.xl';text=path.read_text();start=text.index('export extern "xui_widget_t*" class MessageList ');end=text.index('\nend',start);marker='\n// Document UI declarations generated from xui_document_ui.h.\n';old=text.find(marker,start,end)
if old>=0:text=text[:old]+text[end:];end=old
text=text[:end]+message+text[end:];path.write_text(text)
manifest=root/'lib/bindings/xui/xlang.package.json';j=json.loads(manifest.read_text());sources=j['artifact']['xlSources'];sources[:]=[x for x in sources if x not in ('src/document_ui_records.xl','src/document_ui.xl')];sources+=['src/document_ui_records.xl','src/document_ui.xl'];manifest.write_text(json.dumps(j,indent=2)+'\n')
rows=[]
for f in h['functions']:
 name=f['name'];reason=reasons.get(name,'');state='declared-not-yet-qualified'
 if name not in mapped:
  state='private-lifetime-infrastructure'
  if name.endswith('GetContext'):state='explicit-C';reason='Borrowed Context pointer has no independent native owner to upgrade; use the existing constructor Context'
  elif name.endswith('GetType'):state='explicit-C';reason='Native WidgetType registry identity is not an owned public Widget object'
  elif name=='xuiDocumentFontOutputInit':reason='Fresh native callback storage only; resetting a live output would erase owning Font state'
  elif name.endswith(('SetOpenLink','SetInvalidate')):state='explicit-C';reason='Persistent raw callback/userdata contract; use the typed Owned setter'
  elif name in ('xuiDocumentWebObjectMeasure','xuiDocumentWebObjectDraw'):state='explicit-C';reason='Legacy callback adapter erases provider as void*; typed Renderer Owned callbacks avoid untracked userdata'
  elif name=='xuiDocumentRendererGetRangeRects':state='explicit-C'
  elif any(x in name for x in ('Release','LifetimeWatch','TraceOwnership','OwnershipBusy')):reason='Cold private resource policy/watch/graph contract; language close does not expose a second consuming native owner'
 rows.append(dict(native=name,declaration=f['declaration'],line=f['line'],logical=mapped.get(name,[]),status=state,reason=reason))
api=dict(scope=__doc__,authority=h['header'],authoritySha256=h['sha256'],functions=rows,records=newRecords,constants=constants,notes=['No per-method C forwarding wrapper','Descriptors are synchronous zeroed stack records; Native copies/pins/retains remain real costs','Browser/image APIs are declaration/ABI only, no external I/O execution','Optional NULL variants of the same Native symbol still require separately reviewed physical-signature alias compatibility'])
(root/'docs/xge3-xui3-document-ui-api-map.json').write_text(json.dumps(api,indent=2)+'\n');print('Document UI',len(mapped),'functions /',len(h['functions']),'Native,',len(newRecords),'POD records')
