"""Trace the actual post-field-binding implicit-name producer and its closure.

Only the entry writes nodes,and its three outputs are disjoint from every
helper's node inputs. Classifier admission/original scopes remain intact.
"""
import hashlib,re
from check_native_bridge_catalog import function,block
from ir_node_graph_schema import record,clean

PINS={
 'xl6IrBuildImplicitNameAbis':'300f44c99886e2eb88bc72dc965dec6b0eeba641aef1fb9359f7840d77168457',
 'xl6IrClassifyImplicitNameAbi':'8bb17eb80babb1001379483c860fb489359e4c1b7247cdcc95c6f8b826ad4c78',
 'xlsClassifyOriginal_xl6IrClassifyImplicitNameAbi':'fd294b9daf74f538508bd41c43550fba7f5148cc2ad863e31d0c5fbd79bcb9d4',
 'xl6IrBuildImplicitClassField':'6242598da8f5c29ab389f3b53920318ccea3d7aeb13e1d84f99f0739ee6fd50a',
 'xl6IrNodeGet':'a1d6375145b7123c25224c480e656eac97cf0e136d27f16c0fe07193023de520',
 'xl6IrBuildCallTargetNeedsReceiver':'6cefbf7ead7be65532bfbdc21743a973c27a3866ce7dc0185b3bc03820329fdb',
 'xl6IrBuildEnclosingClass':'b4c23140b0a3a145eb738874463c384cf9acdcb400340d9fd5b59e3166989a87',
 'xl6IrTypeGet':'753e997667f51bbbff554f5ed70be9664f33f94390926062ef81546c5e185dd5',
 'xl6IrCleanupEnclosingCallable':'28761d28fc4a81ddb99e5b02b6555a83c973e1ba712aa0a7f7ee119b5b9b037e',
 'xlsIrImplicitTargetAllowed':'2c3f0ef1a15887bc3049968bf92fcb24a99221a960dad992ca7c6f3cf369a5f8',
}
NODE_FIELDS={'declarationOwnerType','flags','kind','parent','sourceKind','storageType','symbol','targetSymbol','text','typeValueTarget','valueCategory'}
ENTRY_FIELDS={'kind','symbol','targetSymbol','typeValueTarget','valueCategory','storageType','text'}
TYPE_FIELDS={'kind','nominalSymbol'}
OUTPUT_FIELDS={'implicitNameAbi','implicitReceiverType','implicitFieldDeclaration'}

def once(s,a,b):
 if s.count(a)!=1:raise ValueError('Native name-cache seam changed: '+a)
 return s.replace(a,b,1)

def derive(source,header,field_helper):
 bodies={}
 for name in PINS:
  if name=='xl6IrClassifyImplicitNameAbi':
   marker='static XLS_CLASSIFY_INLINE XL6_IrImplicitNameAbi '+name+'('
   if source.count(marker)!=1:raise ValueError('Native classifier wrapper changed')
   body=block(source,source.index(marker))
  else:body=function(field_helper if name=='xlsIrImplicitTargetAllowed'else source,name)
  if hashlib.sha256(body.encode()).hexdigest()!=PINS[name]:raise ValueError('Native name-cache dependency changed: '+name)
  bodies[name]=body
 calls={v for b in bodies.values()for v in re.findall(r'\b((?:xl6Ir|xlsClassify|xlsIr)\w*)\s*\(',b)}
 if calls!=PINS.keys():raise ValueError('Native name-cache transitive closure changed')
 fields={v['name']:v for v in record(clean(header),'XL6_IrNode')}
 helpers='\n'.join(b for n,b in bodies.items()if n!='xl6IrBuildImplicitNameAbis')
 observed=set()
 for name,b in bodies.items():
  if name=='xl6IrBuildImplicitNameAbis':continue
  for alias in re.findall(r'const\s+XL6_IrNode\s*\*\s*(\w+)',b):observed.update(re.findall(r'\b'+alias+r'->(\w+)',b))
 if observed!=NODE_FIELDS or OUTPUT_FIELDS&observed:raise ValueError('Native name input/output disjointness changed')
 entry=bodies['xl6IrClassifyImplicitNameAbi']+'\n'+bodies['xlsIrImplicitTargetAllowed']
 if set(re.findall(r'\bname->(\w+)',entry))!=ENTRY_FIELDS or 'name->text[0]'not in entry:
  raise ValueError('Native name entry projection changed')
 for name in NODE_FIELDS|OUTPUT_FIELDS:
  f=fields[name]
  if f['extent']is not None or(f['pointers']and(name!='text'or f['pointers']!=1 or f['type']!='char')):raise ValueError('Native name field shape changed: '+name)
 types=set()
 for b in bodies.values():
  for alias in re.findall(r'const\s+XL6_IrType\s*\*\s*(\w+)',b):types.update(re.findall(r'\b'+alias+r'->(\w+)',b))
 if types!=TYPE_FIELDS:raise ValueError('Native name type projection changed')
 direct={n for n,b in bodies.items()if '->nodes'in b}
 if direct!={'xl6IrBuildImplicitNameAbis','xl6IrNodeGet'}:raise ValueError('Untraced native name node alias')
 root=bodies['xl6IrBuildImplicitNameAbis']
 if set(re.findall(r'node->(\w+)',root))!=OUTPUT_FIELDS:raise ValueError('Native name output set changed')
 names={n:'xlsNamesNative'+n for n in PINS};names['xl6IrNodeGet']='xlsNamesNodeGet';names['xl6IrTypeGet']='xlsNamesTypeGet'
 generated=[];prototypes=[]
 for name,body in bodies.items():
  if name in('xl6IrNodeGet','xl6IrTypeGet'):continue
  if name=='xl6IrBuildImplicitNameAbis':
   body=once(body,'(XL6_IrModule* module)','(XL6_IrModule* module,size_t first,size_t end,uint64_t* visited)')
   body=once(body,'for ( i = 0; i < module->nodeCount; ++i ) {','for(i=first;i<end;++i){\n        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,0))return 0;\n        ++*visited;')
  if name in('xl6IrClassifyImplicitNameAbi','xlsClassifyOriginal_xl6IrClassifyImplicitNameAbi'):
   if 'const XL6_IrNode* name = xl6IrNodeGet(module, id);'in body:body=once(body,'const XL6_IrNode* name = xl6IrNodeGet(module, id);','const XL6_IrNode* name = xlsNamesEntryGet(module,id);')
   else:body=once(body,'const XL6_IrNode* name = xl6IrNodeGet(module,id);','const XL6_IrNode* name = xlsNamesEntryGet(module,id);')
  if name=='xlsClassifyOriginal_xl6IrClassifyImplicitNameAbi':
   # The entry only consumes scalar admission facts and text nonemptiness.
   # Upgrade before any ancestor/full-spelling/class-field-dependent work.
   body=once(body,'    callableId = xl6IrCleanupEnclosingCallable(module, id);','    name = xlsNamesNodeGet(module,id);\n    callableId = xl6IrCleanupEnclosingCallable(module, id);')
  body=re.sub(r'\b('+'|'.join(PINS)+r')(?=\s*\()',lambda m:names[m[1]],body)
  if not body.startswith('static '):body='static '+body
  prototypes.append(body[:body.index('{')].rstrip()+';');generated.append(body)
 projection='#define XLS_NAME_CACHE_NODE_FIELDS(V,T) '+''.join(('T'if n=='text'else'V')+'('+n+') 'for n in sorted(NODE_FIELDS))+'\n'
 return projection+'\n'.join(prototypes+generated)+'\n',dict(functions=PINS,nodeFields=sorted(NODE_FIELDS),entryFields=sorted(ENTRY_FIELDS),entryText='nonempty only;heavy path upgrades to full projection',queriedTypeFields=sorted(TYPE_FIELDS),outputFields=sorted(OUTPUT_FIELDS),outputsDisjointFromInputs=True,inputsStableDuringPhase=True,directNodeReaders=sorted(direct),scope='Post-field-binding native implicit-name phase;exact IDs;no ordinary integration')
