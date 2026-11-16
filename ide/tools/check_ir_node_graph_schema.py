"""Exercise final-node ownership admission against the actual compiler header."""
from pathlib import Path
import argparse
import json
from ir_node_graph_schema import validate_sources


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frontend',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    paths=[args.frontend/'src/ir/xl6_ir.h']
    foreign=args.frontend/'src/type/xl6_foreign_call_contract.h'
    if foreign.is_file():paths.append(foreign)
    headers={str(p):p.read_text(encoding='utf8')for p in paths}
    original=validate_sources(headers);ir=str(paths[0]);text=headers[ir]
    anchor='    XL6_IrSpan span;'
    if text.count(anchor)!=1:raise ValueError('Native span seam changed; audit tests')
    cases=[
        ('new-owned-pointer',ir,text.replace(anchor,'    char* newOwner;\n'+anchor,1)),
        ('pointer-array',ir,text.replace('char* text;','char* text[2];',1)),
        ('pointer-indirection',ir,text.replace('char* text;','char** text;',1)),
        ('missing-text',ir,text.replace('char* text;','char* spelling;',1)),
        ('missing-literal',ir,text.replace('unsigned char* literalData;','',1)),
        ('changed-literal-pointer',ir,text.replace('unsigned char* literalData;','uint8* literalData;',1)),
        ('hidden-reference',ir,text.replace(anchor,'    UnknownRecord hidden;\n'+anchor,1)),
        ('callback-pointer',ir,text.replace(anchor,'    void (*callback)(void);\n'+anchor,1)),
        ('duplicate-field',ir,text.replace(anchor,'    int flags;\n'+anchor,1)),
        ('span-reference',ir,text.replace('typedef struct XL6_IrSpan {','typedef struct XL6_IrSpan {\n    char* hidden;',1)),
        ('macro-array',ir,text.replace(anchor,'    int values[UNKNOWN_COUNT];\n'+anchor,1)),
    ]
    if foreign.is_file():
        key=str(foreign);cases.append(('foreign-value-reference',key,headers[key].replace('typedef struct XL6_ForeignCallFacts {','typedef struct XL6_ForeignCallFacts {\n    char* hidden;',1)))
    rows=[]
    for name,path,changed in cases:
        if changed==headers[path]:raise ValueError('Mutation did not change native source: '+name)
        candidate=dict(headers);candidate[path]=changed
        try:validate_sources(candidate)
        except ValueError as error:rows.append(dict(name=name,rejected=True,error=str(error)))
        else:raise AssertionError('Ownership schema accepted '+name)
    legacy=dict(headers);legacy[ir]=text.replace('    char* compiledPackageLink;\n','',1)
    if legacy[ir]!=text:
        result=validate_sources(legacy)
        assert result['ownedPointers']=={'text':'char','literalData':'unsigned char'}
        rows.append(dict(name='legacy-without-package-pointer',accepted=True))
    scalar=dict(headers);scalar[ir]=text.replace(anchor,'    uint64 additionalValue;\n'+anchor,1)
    result=validate_sources(scalar)
    assert result['ownedPointers']==original['ownedPointers']and any(r['name']=='additionalValue'for r in result['fields'])
    rows.append(dict(name='additional-copied-scalar',accepted=True))
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/'result.json').write_text(json.dumps(dict(status='passed',actualSchema=original,cases=rows),indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(status='passed',fields=len(original['fields']),cases=len(rows),ownedPointers=original['ownedPointers'])))


if __name__=='__main__':main()
