"""Exercise ownership schema admission against actual compiler dependencies."""
from pathlib import Path
import argparse,json,re
from build_lsp_core import dependencies
from ir_table_schema import Schema,clean

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--dependencies',type=Path,required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args()
 paths=[p for p in dependencies(args.dependencies.read_text(encoding='utf8'))if p.suffix=='.h'and p.name.startswith('xl6_')]
 headers={str(p):p.read_text(encoding='utf8')for p in paths};baseline=Schema(headers).build()
 texts={p:clean(s)for p,s in headers.items()};cases=[]
 def changed(record,old,new):
  pattern=r'typedef\s+struct\s+'+record+r'\s*\{([^{}]*)\}'
  matches=[(p,m)for p,s in texts.items()for m in re.finditer(pattern,s,re.S)]
  if len(matches)!=1:raise ValueError('Selected record seam changed: '+record)
  path,m=matches[0];body=m[1]
  if body.count(old)!=1:raise ValueError('Selected field seam changed: '+record+' '+old)
  s=texts[path];result=dict(texts);result[path]=s[:m.start(1)]+body.replace(old,new,1)+s[m.end(1):];return result
 for label,record,old,new in [
  ('unowned-record-pointer','XL6_IrType','char* name;','char* name; char* unowned;'),
  ('pointer-array','XL6_IrType','char* displayName;','char* displayName[2];'),
  ('pointer-depth','XL6_IrType','XL6_IrTypeId* params;','XL6_IrTypeId** params;'),
  ('wrong-vector-count','XL6_IrType','size_t paramCount;','uint32 paramCount;'),
  ('function-pointer','XL6_IrType','char* name;','char* name; void (*callback)(void);'),
  ('hidden-value-reference','XL6_IrSequenceLayout','XL6_IrSequenceLayoutAbi abi;','XL6_IrSequenceLayoutAbi abi; char* hidden;'),
  ('hidden-runtime-map-reference','XL6_IrRuntimeTypeMap','uint32 abiVersion;','uint32 abiVersion; char* hidden;'),
  ('unknown-value-record','XL6_IrType','char* name;','char* name; UnknownRecord hidden;'),
  ('duplicate-field','XL6_IrModule','XL6_IrStage stage;','XL6_IrStage stage; int stage;'),
  ('root-count-representation','XL6_IrModule','size_t classFieldCount;','uint32 classFieldCount;'),
  ('type-pool-pointer-array','XL6_IrTypePool','XL6_IrType* items;','XL6_IrType* items[2];'),
  ('node-arena-pointer-depth','XL6_IrModule','XL6_IrNode* nodes;','XL6_IrNode** nodes;'),
  ('new-root-pointer-without-extent','XL6_IrModule','XL6_IrStage stage;','XL6_IrStage stage; char* hidden;'),
 ]:cases.append((label,changed(record,old,new),False))
 cases.append(('additional-copied-scalar',changed('XL6_IrType','char* name;','char* name; uint64 addedValue;'),True))
 cases.append(('additional-fixed-value-array',changed('XL6_IrType','char* name;','char* name; uint32 addedValues[3];'),True))
 cases.append(('additional-counted-scalar-table',changed('XL6_IrModule','XL6_IrStage stage;','XL6_IrStage stage; uint32* extraValues; size_t extraValueCount; size_t extraValueCapacity;'),True))
 rows=[]
 for label,candidate,accept in cases:
  try:result=Schema(candidate).build()
  except ValueError as error:
   if accept:raise
   rows.append(dict(name=label,rejected=True,error=str(error)))
  else:
   if not accept:raise AssertionError('Accepted '+label)
   rows.append(dict(name=label,accepted=True,pointerFields=result['pointerFields']))
 args.output.mkdir(parents=True,exist_ok=False)
 (args.output/'result.json').write_text(json.dumps(dict(status='passed',actualSchema=baseline,cases=rows),indent=2)+'\n',encoding='utf8')
 print(json.dumps(dict(status='passed',tables=len(baseline['tables']),records=len(baseline['records']),pointerFields=baseline['pointerFields'],cases=len(rows))))

if __name__=='__main__':main()
