"""Check closed type roles against the selected native dependency headers."""
from pathlib import Path
import argparse,hashlib,json,re
from build_lsp_core import dependencies
from ir_table_schema import clean
from ir_type_schema import generate

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--dependencies',type=Path,required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
 paths=[p for p in dependencies(args.dependencies.read_text(encoding='utf8'))if p.suffix=='.h'and p.name.startswith('xl6_')]
 headers={str(p):p.read_text(encoding='utf8')for p in paths};baseline=generate(headers,args.output/'baseline');texts={p:clean(s)for p,s in headers.items()}
 def changed(record,old,new):
  pattern=r'typedef\s+struct\s+'+record+r'\s*\{([^{}]*)\}'
  hits=[(p,m)for p,s in texts.items()for m in re.finditer(pattern,s,re.S)]
  if len(hits)!=1:raise ValueError('Selected type record seam changed: '+record)
  p,m=hits[0];body=m[1]
  if body.count(old)!=1:raise ValueError('Selected type field seam changed: '+old)
  result=dict(texts);result[p]=texts[p][:m.start(1)]+body.replace(old,new,1)+texts[p][m.end(1):];return result
 rows=[]
 for label,record,old,new in [
  ('new-type-value','XL6_IrType','uint64 hash;','uint64 hash; uint32 added;'),
  ('removed-type-reference','XL6_IrType','XL6_IrTypeId foreignBase;',''),
  ('reference-width','XL6_IrType','XL6_IrTypeId foreignBase;','uint64 foreignBase;'),
  ('reference-pointer','XL6_IrType','XL6_IrTypeId foreignBase;','XL6_IrTypeId* foreignBase;'),
  ('source-type-role','XL6_IrType','XL6_TypeId sourceTypeId;','uint64 sourceTypeId;'),
  ('value-becomes-reference','XL6_IrType','uint64 typeValueId;','XL6_IrTypeId typeValueId;'),
  ('nested-new-value','XL6_IrSequenceLayout','XL6_IrTypeId elementType;','XL6_IrTypeId elementType; uint32 added;'),
  ('nested-reference-width','XL6_IrSequenceLayout','XL6_IrTypeId elementType;','uint64 elementType;'),
  ('nested-value-becomes-reference','XL6_IrSequenceLayout','bool nullable;','XL6_IrTypeId nullable;'),
  ('derived-name-extent','XL6_IrType','char cName[64];','char cName[65];'),
  ('parameter-reference-type','XL6_IrType','XL6_IrTypeId* params;','uint32* params;'),
  ('vector-count-type','XL6_IrType','size_t paramCount;','uint32 paramCount;'),
  ('owned-text-depth','XL6_IrType','char* foreignPolicy;','char** foreignPolicy;'),
  ('new-pointer','XL6_IrType','uint64 hash;','uint64 hash; char* added;'),
 ]:
  try:generate(changed(record,old,new),args.output/label)
  except ValueError as error:rows.append(dict(name=label,rejected=True,error=str(error)))
  else:raise AssertionError('Accepted unaudited type layout: '+label)
 result=dict(status='passed',roles=len(baseline['referenceRoles']),comparisonFields=baseline['completeComparisonFields'],cases=rows,actualSchema=baseline,toolSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
 (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
 print(json.dumps(dict(status='passed',roles=result['roles'],comparisonFields=result['comparisonFields'],cases=len(rows))))

if __name__=='__main__':main()
