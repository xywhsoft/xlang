"""Generate the two actual adjacent handle producers with a shared read proof.

Native order remains all binds then all projects. Inputs are immutable across
both phases;only independent scalar output tables and their capacities grow.
"""
import hashlib,re
from check_native_bridge_catalog import function
from ir_node_graph_schema import record,clean
PINS={'xl6IrBuildNativeHandleProjects': '3254ce2deca373cd5a200bdb683cc592866456b71f6c32221a29108879d702de', 'xl6IrBuildFindNativeHandleField': 'b5b5536ce330a56b039d223ab7cf1786c305bab6e78e015a9cc83d1ff57df6b4', 'xl6IrNodeGet': 'a1d6375145b7123c25224c480e656eac97cf0e136d27f16c0fe07193023de520', 'xl6IrSymbolNodeGet': 'c56567357d2b623c7a7609dc67866723f087653831307aa8e35790e425112676', 'xl6IrTypeGet': '753e997667f51bbbff554f5ed70be9664f33f94390926062ef81546c5e185dd5', 'xlsIrTypeGrow': '0db57dec2868dad547a396eb3fc4b2c201f77947792234b6b7ee690a06fc5446', 'xl6IrGrow': '8c42062ff6f5d3469924af9d9122176b0652ef123229e738750d8fee0d460cf5', 'xlsIrTypeRealloc': '63fd46eb47b72d51bb9bae9f2833a19cf1c2f2e8db567396cb5d1b108fd66453', 'xlsIrTypePointer': '80daad1d33ee99a20c6ead98fed1f0df7a5e1e28f0a480fd893394a1bf29b65e', 'xl6IrBuildNativeHandleBinds': 'c64166bd35ea2e4f40415ca254f2724c5e3aaba40ee28daacfce0f8fc6f61b90', 'xl6IrNativeHandleDestroySymbol': '60802f902ca95d3e33e7ee837070be69c24870a61a08bf95e48c3a57b70cff90', 'xl6IrManagedNativeConstructorField': 'a118f4940dad5581a21afa11f53e44282aeadb7f229947e9e0aebf4a448f9d39', 'xl6IrManagedNativeField': '1e8c617b3a5c0770fcdc142db02f67651131c69428ad07fb3f779b461e14e538', 'xl6IrBuildClassLayoutByType': 'd351bedb32fd51b7d6c0b5bd40716b4e14bfb2c31906effb40cde3e8cc2dc375'}
ROOTS=('xl6IrBuildNativeHandleBinds','xl6IrBuildNativeHandleProjects')
RESOURCE={'xlsIrTypeGrow','xl6IrGrow','xlsIrTypeRealloc','xlsIrTypePointer'}
NODE_FIELDS={'callKind','childCount','firstChild','flags','kind','parent','sourceKind','storageType','symbol','targetSymbol','text','type','valueCategory'}
TYPE_FIELDS={'cAbi','kind','nominalSymbol','ownershipAbi','paramCount','params'}
def once(s,a,b):
 if s.count(a)!=1:raise ValueError('Native handle group seam changed: '+a)
 return s.replace(a,b,1)
def derive(source,header,resource):
 bodies={n:function(resource if n.startswith('xlsIrType')else source,n)for n in PINS}
 for n,b in bodies.items():
  if hashlib.sha256(b.encode()).hexdigest()!=PINS[n]:raise ValueError('Native handle group dependency changed: '+n)
 calls={v for b in bodies.values()for v in re.findall(r'\b((?:xl6Ir|xlsIr)\w*)\s*\(',b)}
 if calls!=PINS.keys():raise ValueError('Native handle group closure changed')
 fields={v['name']:v for v in record(clean(header),'XL6_IrNode')}
 if set(re.findall(r'->(\w+)', '\n'.join(bodies.values())))&fields.keys()!=NODE_FIELDS:raise ValueError('Native handle node projection changed')
 for n in NODE_FIELDS:
  f=fields[n]
  if f['extent']is not None or(f['pointers']and(n!='text'or f['pointers']!=1 or f['type']!='char')):raise ValueError('Native handle input shape changed')
 types=set()
 for b in bodies.values():
  for alias in re.findall(r'const\s+XL6_IrType\s*\*\s*(\w+)',b):types.update(re.findall(r'\b'+alias+r'->(\w+)',b))
 if types!=TYPE_FIELDS:raise ValueError('Native handle type projection changed')
 direct={n for n,b in bodies.items()if '->nodes'in b}
 if direct!=set(ROOTS)|{'xl6IrNodeGet','xl6IrNativeHandleDestroySymbol'}:raise ValueError('Untraced native handle node alias')
 for root,table in zip(ROOTS,('nativeHandleBinds','nativeHandleProjects')):
  if set(re.findall(r'module->(nativeHandle\w+)',bodies[root]))!={table,table.removesuffix('s')+'Count',table.removesuffix('s')+'Capacity'}:raise ValueError('Native handle output/read table changed')
 for n,b in bodies.items():
  if n not in ROOTS and n not in RESOURCE and re.search(r'->nativeHandle(?:Bind|Project)',b):raise ValueError('Output table is a producer input')
 for typename in('XL6_IrNativeHandleBind','XL6_IrNativeHandleProject'):
  if any(f['pointers']or f['extent']is not None for f in record(clean(header),typename)):raise ValueError('Native handle output is no longer independent scalar records')
 names={n:'xlsHGNative'+n for n in PINS if n not in RESOURCE}
 names['xl6IrNodeGet']='xlsHGNodeGet';names['xl6IrTypeGet']='xlsHGTypeGet'
 code=[];prototypes=[]
 for n,b in bodies.items():
  if n in RESOURCE or n in('xl6IrNodeGet','xl6IrTypeGet'):continue
  if n in ROOTS:
   b=once(b,'(XL6_IrModule* module)','(XL6_IrModule* module,size_t first,size_t end,uint64_t* visited)')
   b=once(b,'for ( i = 0; i < module->nodeCount; ++i ) {','for(i=first;i<end;++i){\n        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,0))return 0;\n        ++*visited;')
   b=once(b,'&module->nodes[i]','xlsHGEntryGet(module,(XL6_IrNodeId)(i+1))')
  b=re.sub(r'\b('+'|'.join(names)+r')(?=\s*\()',lambda m:names[m[1]],b)
  if not b.startswith('static '):b='static '+b
  prototypes.append(b[:b.index('{')].rstrip()+';');code.append(b)
 projection='#define XLS_HG_NODE_FIELDS(V,T) '+''.join(('T'if n=='text'else'V')+'('+n+') 'for n in sorted(NODE_FIELDS))+'\n'
 return projection+'\n'.join(prototypes+code)+'\n',dict(functions=PINS,nodeFields=sorted(NODE_FIELDS),queriedTypeFields=sorted(TYPE_FIELDS),phases=[28,29],inputsStableAcrossBothPhases=True,independentScalarOutputs=True,nativeOrder='All bind ranges,then all project ranges',lifecycleScanProof='Common ordered native-special candidates including child/name facts;native ancestor/type reads traced',scope='One exact input validation per range shared by two actual producers;no ordinary integration')
