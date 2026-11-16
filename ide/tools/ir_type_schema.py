"""Closed native type-record roles for relocation and complete comparison."""
from pathlib import Path
from ir_table_schema import Schema

TYPE_REFS=set('persistencePayloadType messageSchemaType treeKeySchemaType treeValueSchemaType collectionValueSchemaType foreignBase foreignOwner callableResultType'.split())
TEXTS=set('name displayName foreignHeader foreignRecordSource foreignPolicy foreignScope foreignWatch foreignDomain'.split())
VALUES=set('kind cAbi collectionAbi collectionCarrierAbi runtimeResourceAbi ownershipAbi nullAbi persistenceValueAbi persistenceRestoreScalarAbi persistenceRestoreCarrierAbi persistenceWireFormat integerTarget nativeProductAbi nativeValueAbi nativeSlotAbi setKeyAbi dictKeyAbi xvalueAdmissionAbi xvalueAdmissionNullable foreignRecordAbi xvalueAdmissionTypeValueId abiId typeValueId foreignLifetime nativeArrayExtent callableErasureAbi paramCount returnCount'.split())
SPECIAL={'sequenceLayout','sourceTypeId','nominalSymbol','cName','errorResultCName','hash','params','returns','parameterModes','parameterAbis'}
SEQUENCE_REFS={'elementType','baseElementType','childElementType'}
SEQUENCE_VALUES=set('abi role slotAbi codecAbi transferAbi admissionAbi nullable defaultAbi childRole childSlotAbi childTransferAbi'.split())
VECTOR_TYPES={'params':('XL6_IrTypeId','paramCount'),'returns':('XL6_IrTypeId','returnCount'),
              'parameterModes':('XL6_IrParameterMode','paramCount'),'parameterAbis':('XL6_IrParameterAbi','paramCount')}

def generate(headers,directory):
 schema=Schema(headers);ownership=schema.build();rows=schema.record('XL6_IrType');sequence=schema.record('XL6_IrSequenceLayout')
 names={r['name']for r in rows};expected=TYPE_REFS|TEXTS|VALUES|SPECIAL
 if names!=expected:raise ValueError('Audit native type roles: missing='+str(sorted(expected-names))+' new='+str(sorted(names-expected)))
 if {r['name']for r in sequence}!=SEQUENCE_REFS|SEQUENCE_VALUES:raise ValueError('Audit native sequence layout roles')
 refs=[];compare=[]
 def representation(row,ty,pointers=0,extent=None):
  if (row['type'],row['pointers'],row['extent'])!=(ty,pointers,extent):raise ValueError('Native type field representation changed: '+row['name'])
 for r in rows:
  n=r['name']
  if n in TYPE_REFS:representation(r,'XL6_IrTypeId');refs.append((n,'REFERENCE'));compare.append('TYPE_VALUE('+n+');')
  elif n=='nominalSymbol':representation(r,'XL6_SymbolId');refs.append((n,'SYMBOL'));compare.append('TYPE_VALUE('+n+');')
  elif n=='sequenceLayout':
   representation(r,'XL6_IrSequenceLayout')
   for field in sequence:
    path=n+'.'+field['name']
    if field['name']in SEQUENCE_REFS:representation(field,'XL6_IrTypeId');refs.append((path,'REFERENCE'))
    elif field['pointers']or field['extent']or field['type']in ('XL6_IrTypeId','XL6_TypeId','XL6_SymbolId'):raise ValueError('Sequence value became a reference: '+path)
    compare.append('TYPE_VALUE('+path+');')
  elif n in TEXTS:representation(r,'char',1);compare.append('TYPE_TEXT('+n+');')
  elif n in VECTOR_TYPES:
   ty,count=VECTOR_TYPES[n];representation(r,ty,1);compare.append('TYPE_VECTOR('+n+','+count+');')
  elif n in ('cName','errorResultCName'):representation(r,'char',0,'64'if n=='cName'else'48');compare.append('TYPE_ARRAY('+n+');')
  else:
   if n=='sourceTypeId':representation(r,'XL6_TypeId')
   elif n=='abiId':representation(r,'XL6_TypeAbiId')
   elif r['pointers']or r['extent']or r['type']in ('XL6_IrTypeId','XL6_TypeId','XL6_SymbolId'):raise ValueError('Type value became a reference: '+n)
   compare.append('TYPE_VALUE('+n+');')
 directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
 (directory/'ir-type-reference.inc').write_text('/* Selected native type reference roles. */\n'+''.join('TYPE_REFERENCE('+n+','+d+');\n'for n,d in refs),encoding='utf8')
 (directory/'ir-type-compare.inc').write_text('/* Compare every selected native type value/payload,not padding. */\n'+'\n'.join(compare)+'\n',encoding='utf8')
 return dict(schema=1,fields=rows,sequenceFields=sequence,referenceRoles=refs,vectorReferences=['params','returns'],
             conditionalSourceReference='sourceTypeId for TYPE_PARAM',currentProducerDerived=['hash','types.slots','cName','errorResultCName','sourceTypeId except TYPE_PARAM'],
             completeComparisonFields=len(compare),ownershipSchema=ownership,
             scope='Current validated metadata,explicit maps and exact semantic equality;not dependency eligibility or selective production')
