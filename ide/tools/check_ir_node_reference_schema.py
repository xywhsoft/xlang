"""Reject unaudited native node reference layouts using actual dependencies."""
from pathlib import Path
import argparse,json,re,hashlib
from build_lsp_core import dependencies
from ir_node_reference_schema import generate
from ir_node_graph_schema import clean

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--dependencies',type=Path,required=True)
 parser.add_argument('--output',type=Path,required=True)
 args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
 paths=[p for p in dependencies(args.dependencies.read_text(encoding='utf8'))
        if p.name in ('xl6_ir.h','xl6_foreign_call_contract.h')]
 headers={str(p):p.read_text(encoding='utf8')for p in paths}
 baseline=generate(headers,args.output/'baseline.inc');texts={p:clean(t)for p,t in headers.items()}
 def changed(record,old,new):
  pattern=r'typedef\s+struct\s+'+record+r'\s*\{([^{}]*)\}'
  matches=[(p,m)for p,t in texts.items()for m in re.finditer(pattern,t,re.S)]
  if len(matches)!=1:raise ValueError('Selected reference record seam changed: '+record)
  p,m=matches[0];body=m[1]
  if body.count(old)!=1:raise ValueError('Selected reference field seam changed: '+old)
  result=dict(texts);result[p]=texts[p][:m.start(1)]+body.replace(old,new,1)+texts[p][m.end(1):];return result
 rows=[]
 for label,record,old,new in [
  ('added-scalar-needs-role','XL6_IrNode','size_t literalSize;','size_t literalSize; uint32 added;'),
  ('added-reference-needs-role','XL6_IrNode','size_t literalSize;','size_t literalSize; XL6_IrTypeId added;'),
  ('removed-type-role','XL6_IrNode','XL6_IrTypeId type;',''),
  ('changed-type-representation','XL6_IrNode','XL6_IrTypeId type;','uint64 type;'),
  ('value-becomes-reference','XL6_IrNode','size_t literalSize;','XL6_IrNodeId literalSize;'),
  ('region-width','XL6_IrNode','uint32 region;','uint64 region;'),
  ('field-index-representation','XL6_IrNode','size_t unionField;','uint32 unionField;'),
  ('owned-pointer','XL6_IrNode','char* text;','char** text;'),
  ('nested-new-value-needs-role','XL6_ForeignCallFacts','uint32 rollbackSymbol;','uint32 rollbackSymbol; uint32 added;'),
  ('nested-rollback-width','XL6_ForeignCallFacts','uint32 rollbackSymbol;','uint64 rollbackSymbol;'),
  ('nested-pointer','XL6_ForeignCallFacts','uint32 rollbackSymbol;','uint32* rollbackSymbol;'),
  ('coordinate-width','XL6_IrSpan','int line;','int64 line;'),
 ]:
  candidate=changed(record,old,new)
  try:generate(candidate,args.output/(label+'.inc'))
  except ValueError as error:rows.append(dict(name=label,rejected=True,error=str(error)))
  else:raise AssertionError('Accepted unaudited reference schema: '+label)
 result=dict(status='passed',roles=len(baseline['roles']),nativeFields=len(baseline['fields']),cases=rows,
             actualSchema=baseline,toolSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
 (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
 print(json.dumps(dict(status='passed',roles=result['roles'],cases=len(rows))))

if __name__=='__main__':main()
