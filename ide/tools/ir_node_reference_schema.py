"""Closed native node reference roles for exact namespace relocation."""
from pathlib import Path
import json
from ir_node_graph_schema import validate_sources

TYPES=set('declarationOwnerType operatorExecutionType type storageType typeValueTarget implicitReceiverType namedCallableSignatureType loopResourceType loopProtocolIterableReceiverType loopProtocolIteratorReceiverType loopProtocolIteratorType loopProtocolNextResultType loopProtocolItemType loopGeneratorItemType'.split())
SYMBOLS=set('symbol targetSymbol compoundOperatorSymbol constructorReceiverSymbol namedCallableTargetSymbol loopProtocolIterTarget loopProtocolNextTarget'.split())
NODES={'implicitFieldDeclaration','parent'}
REGIONS={'region','targetRegion'}
VALUES=set('kind sourceKind flags compiledPackageError compiledPackageRoot literalSize literalScalar literalDecoded opcode operatorErrorAbi operatorOperandAbis castAbi castErrorAbi implicitConversionRuntimeOp implicitConversionErrorAbi callKind cOwnedCompareOp implicitNameAbi namedCallableAbi memberAbi projectionErrorAbi evaluationErrorAbi constructionErrorAbi generatorClosePointAbi indexReceiverGate indexWriteAbi indexWriteErrorAbi loopAbi loopKeyAbi loopItemAbi loopKeyLifetimeAbi loopItemLifetimeAbi loopErrorAbi loopResourceAbi lvalueKind exitKind valueCategory ownership ownershipPlanned scopeDepth targetScopeDepth'.split())
PAYLOADS={'text','compiledPackageLink','literalData'}
SPECIAL={'foreignCall','unionField','firstChild','childCount','span'}
FOREIGN_VALUES=set('constAggregateAddresses staticCallbackAddresses textViews bytesViews terminatedStrings physicalPairs retainedCallbacks controlViews borrowedViewResult valueOutput receiverDereference inplaceOutput inputMappingFacts spanPrefixCount resultOwnerIndexPlusOne outputIndexPlusOne successCode'.split())

def generate(headers,path):
 ownership=validate_sources(headers);rows=ownership['fields'];names={r['name']for r in rows}
 expected=TYPES|SYMBOLS|NODES|REGIONS|VALUES|PAYLOADS|SPECIAL
 if names!=expected:raise ValueError('Audit native node reference roles: missing='+str(sorted(expected-names))+' new='+str(sorted(names-expected)))
 foreign=ownership['valueRecords'].get('XL6_ForeignCallFacts',[])
 if {r['name']for r in foreign}!=FOREIGN_VALUES|{'rollbackSymbol'}:raise ValueError('Audit native foreign-call reference roles')
 span=ownership['valueRecords'].get('XL6_IrSpan',[])
 if [(r['name'],r['type'],r['pointers'],r['extent'])for r in span]!=[('line','int',0,None),('column','int',0,None)]:raise ValueError('Audit native node coordinates')
 roleTypes={'TYPE':'XL6_IrTypeId','SYMBOL':'XL6_SymbolId','NODE':'XL6_IrNodeId','REGION':'uint32','AGGREGATE_FIELD':'size_t'}
 roles={}
 for row in rows:
  name=row['name'];role='TYPE'if name in TYPES else'SYMBOL'if name in SYMBOLS else'NODE'if name in NODES else'REGION'if name in REGIONS else'AGGREGATE_FIELD'if name=='unionField'else None
  if role:
   if row['type']!=roleTypes[role]or row['pointers']or row['extent']is not None:raise ValueError('Native reference representation changed: '+name)
   roles[name]=role
  elif row['type']in ('XL6_IrTypeId','XL6_SymbolId','XL6_IrNodeId'):raise ValueError('Native value became a reference: '+name)
 rollback=next(r for r in foreign if r['name']=='rollbackSymbol')
 if rollback['type']!='uint32'or rollback['pointers']or rollback['extent']is not None:raise ValueError('Foreign rollback symbol representation changed')
 roles['foreignCall.rollbackSymbol']='SYMBOL'
 lines=['/* Generated from selected native node reference roles. */']
 for field,role in roles.items():
  lines.append('LINK_REFERENCE('+field+','+role+');')
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n',encoding='utf8')
 return dict(schema=1,roles=roles,fields=rows,foreignFields=foreign,ownershipSchema=ownership,
   sentinels=dict(NODE=0,TYPE=0,SYMBOL=0,REGION=0,AGGREGATE_FIELD='SIZE_MAX'),
   scope='Exact native reference relocation;caller semantic eligibility remains mandatory')
