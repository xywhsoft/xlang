"""Clone the complete reviewed call-table producer with traced node reads.

The cached phase never executes the original producer to qualify a hit.
All17transitive native bodies and the complete source/header cohort must match.
NodeGet and TypeGet are trace gateways;the entry has its own smaller projection.
Every children read belongs to a traced node's complete slice. The helper node
projection is derived from the real declaration and all pinned producer bodies.
The common context covers all other semantic reads;final contracts always run.
"""
import hashlib,re
from check_native_bridge_catalog import function
from ir_node_graph_schema import record,clean

PINS={
 'xl6IrBuildCallSites':'39432f3f294f03ecb8092654e944305e4f9c1f4c42db5b9b615cc27c578889b2',
 'xl6IrBuildSyntaxCallSite':'895a00e2cd4849ff070daf530e182dc3747d9e378865097a6831ffa75662d2d3',
 'xl6IrSyntaxIndexArgumentAt':'dcd265a657c9afd740b70ee0527ed5666fbd2545a8ddb6837eb86f955101d662',
 'xl6IrNodeGet':'a1d6375145b7123c25224c480e656eac97cf0e136d27f16c0fe07193023de520',
 'xl6IrBuildCallSiteDispatchSlot':'440aca2e949ccecd85ef65620c8fd3f7eeca2eca6bbda72f6a612746ddd0fd7a',
 'xl6IrTypeGet':'753e997667f51bbbff554f5ed70be9664f33f94390926062ef81546c5e185dd5',
 'xl6IrNodeIsPropertyUpdate':'d4e0e086d0f7353194404008739e7914c450ba8187c5e98416c2cfbdedf180a1',
 'xl6IrCallVariadicAbiForParameter':'43135c562a52dd7fa937648c9ef70b5e2af113d7845b003fe776db426d6a0ec3',
 'xl6IrSyntaxIndexArgumentCount':'a1c73b87cc63bafa2ade69af5c29541b7b6d519bea0e19833e6de90d3531ea39',
 'xl6IrCallParameterListGet':'19a297c997d0547f0be1eb1a5c4d721c595bc3ac5ca670aa47766c9bca7e66cf',
 'xl6IrBuildCallTargetNeedsReceiver':'6cefbf7ead7be65532bfbdc21743a973c27a3866ce7dc0185b3bc03820329fdb',
 'xl6IrBuildEnclosingClass':'b4c23140b0a3a145eb738874463c384cf9acdcb400340d9fd5b59e3166989a87',
 'xl6IrNodeIsResolvedSyntaxCall':'be380478f5504624e5a1a5833c67423005f93bdfdf51f6709d6f92c65284e465',
 'xl6IrBuildCallSite':'8210edeea32a23d47e362467084aa2540ce9863373838d5c6681a95d6003d8a4',
 'xl6IrBuildFunctionValueCallSite':'9956efbc0716250142c36dc06b67e280bf14f9b56ba94e275da7018e736cc2db',
 'xl6IrBuildCallableVariadicAbi':'d9058fea8640cdd44dc49ae16a970e922c1084b1e7d9ab77255e8af236b2f01a',
 'xl6IrIntrinsicCallGet':'326e56d70701a2903a93a06bccf16e05745e89ecda1a477431f6e4e887497057',
}
ENTRY_FIELDS={'kind','targetSymbol','callKind','opcode','lvalueKind'}
TYPE_FIELDS={'kind','cAbi','callableErasureAbi','paramCount','params','parameterModes','parameterAbis'}
NODE_FIELDS={'callKind','childCount','firstChild','flags','implicitNameAbi','kind','lvalueKind','opcode','parent','sourceKind','storageType','targetSymbol','text','type'}
def once(s,a,b):
 if s.count(a)!=1:raise ValueError('Native call cache seam changed: '+a)
 return s.replace(a,b,1)
def derive(source,header):
 bodies={n:function(source,n)for n in PINS}
 for n,b in bodies.items():
  if hashlib.sha256(b.encode()).hexdigest()!=PINS[n]:raise ValueError('Native call cache dependency changed: '+n)
 calls={v for b in bodies.values()for v in re.findall(r'\b(xl6Ir\w*)\s*\(',b)}-{'xl6IrBuildCallSiteContracts'}
 if calls!=PINS.keys():raise ValueError('Native call cache transitive closure changed')
 entry='\n'.join(bodies[n]for n in('xl6IrNodeIsResolvedSyntaxCall','xl6IrNodeIsPropertyUpdate'))
 if set(re.findall(r'\bnode->(\w+)',entry))|{'kind'}!=ENTRY_FIELDS:
  raise ValueError('Native entry field projection changed')
 observed=set()
 for b in bodies.values():
  for alias in re.findall(r'const\s+XL6_IrType\s*\*\s*(\w+)',b):
   observed.update(re.findall(r'\b'+re.escape(alias)+r'->(\w+)',b))
 if observed!=TYPE_FIELDS:raise ValueError('Native queried-type field projection changed')
 fields={v['name']:v for v in record(clean(header),'XL6_IrNode')}
 node_reads=set(re.findall(r'->(\w+)', '\n'.join(bodies.values())))&fields.keys()
 if node_reads!=NODE_FIELDS:raise ValueError('Native helper node field projection changed')
 for name in NODE_FIELDS:
  row=fields[name]
  if row['extent']is not None or(row['pointers']and(name!='text'or row['pointers']!=1 or row['type']!='char')):
   raise ValueError('Native node projection value shape changed: '+name)
 direct={n for n,b in bodies.items()if '->nodes'in b}
 if direct!={'xl6IrBuildCallSites','xl6IrNodeGet'}:raise ValueError('Untraced native node alias')
 names={n:'xlsCallsNative'+n.removeprefix('xl6Ir')for n in PINS}
 names['xl6IrNodeGet']='xlsCallsNodeGet'
 names['xl6IrTypeGet']='xlsCallsTypeGet'
 generated=[];prototypes=[]
 for n,b in bodies.items():
  if n in('xl6IrNodeGet','xl6IrTypeGet'):continue
  if n=='xl6IrBuildCallSites':
   b=once(b,'(XL6_IrModule* module)','(XL6_IrModule* module,size_t first,size_t end,uint64_t* visited)')
   b=once(b,'for ( i = 0; i < module->nodeCount; ++i ) {','for (i=first;i<end;++i) {\n        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,0))return 0;\n        ++*visited;')
   if (b.count('module->nodes[i]')!=3 or b.count('module->nodes[i].kind')!=2 or
       b.count('&module->nodes[i]')!=1):raise ValueError('Untraced native entry read')
   b=b.replace('&module->nodes[i]','xlsCallsEntryGet(module,(XL6_IrNodeId)(i+1))').replace('module->nodes[i].','xlsCallsEntryGet(module,(XL6_IrNodeId)(i+1))->')
   b=once(b,'return xl6IrBuildCallSiteContracts(module);','return 1;')
  b=re.sub(r'\b('+'|'.join(PINS)+r')(?=\s*\()',lambda m:names[m[1]],b)
  if not b.startswith('static '):b='static '+b
  prototypes.append(b[:b.index('{')].rstrip()+';');generated.append(b)
 projection='#define XLS_CALL_CACHE_NODE_FIELDS(V,T) '+''.join(('T'if n=='text'else'V')+'('+n+') 'for n in sorted(NODE_FIELDS))+'\n'
 return projection+'\n'.join(prototypes+generated)+'\n',dict(functions=PINS,nodeGateway='xlsCallsNodeGet',typeGateway='xlsCallsTypeGet',entryFields=sorted(ENTRY_FIELDS),helperNodeFields=sorted(NODE_FIELDS),queriedTypeFields=sorted(TYPE_FIELDS),directNodeReaders=sorted(direct),readSet='Five entry fields;14audited helper node fields/text/children;actual queried type projection;symbol map,intrinsic origins,class/protocol dispatch projection,initial call rows',output='Independent call sites/arguments;relative argument slices;original final contracts',scope='Call-table phase only,exact IDs;no ordinary integration')
