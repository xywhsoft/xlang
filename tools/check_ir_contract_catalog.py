"""Compare every compiled declaration field with the production compact catalog."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import time
from generate_ir_contract_catalog import schema, compile_declarations, generate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--skip-mutations", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    out = (args.directory or root / "build/test/ir-contract-catalog").resolve()
    out.mkdir(parents=True, exist_ok=True)
    fields = schema(root)
    rows = compile_declarations(root, out / "declarations", args.cc, fields)
    expected, stats = generate(fields, rows)
    data = root / "src/ir/xl6_ir_direct_bridge_data.inc"
    if data.read_text(encoding="utf-8") != expected:
        raise RuntimeError("stale native contract catalog")
    prefix = '''#include "src/ir/xl6_ir.h"
#include "src/ir/xl6_ir_direct_bridge_schema.h"
#include "src/ir/xl6_ir_direct_bridge_enums.inc"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "src/ir/xl6_ir_direct_bridge_declarations.inc"
#pragma GCC diagnostic pop
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include CATALOG_FILE
#include "src/ir/xl6_ir_direct_bridge_lookup.inc"
static int failure(const char* name,const char* field){fprintf(stderr,"%s: %s\\n",name,field);return 1;}
static int equalString(const char* a,const char* b){return a&&b?!strcmp(a,b):a==b;}
static const XL6_IrDirectBridgeDeclaration* legacy(const char* name){
    for(size_t i=0;i<sizeof(xl6IrDirectBridgeDeclarations)/sizeof(xl6IrDirectBridgeDeclarations[0]);++i)
        if(!strcmp(name,xl6IrDirectBridgeDeclarations[i].name))return &xl6IrDirectBridgeDeclarations[i];
    return NULL;
}
#define CHECK_INT(A,B,Field) do{if((A)!=(B))return failure(old->name,Field);}while(0)
#define CHECK_STRING(A,B,Field) do{if(!equalString((A),(B)))return failure(old->name,Field);}while(0)
#define CHECK_ENUM(A,B,Field) CHECK_INT(A,B,Field)
int main(void){
    size_t count=sizeof(xl6IrDirectBridgeDeclarations)/sizeof(xl6IrDirectBridgeDeclarations[0]);
    if(count!=sizeof(xl6IrDirectBridgeContracts)/sizeof(xl6IrDirectBridgeContracts[0]))return 2;
    for(size_t i=0;i<count;++i){
        const XL6_IrDirectBridgeDeclaration* old=&xl6IrDirectBridgeDeclarations[i];
        const XL6_IrDirectBridgeContract* current=xl6IrDirectBridgeContract(old->name);
        if(!current)return failure(old->name,"missing lookup");
#define XL6_DIRECT_BASE(Encoding,Type,Name) CHECK_##Encoding(old->Name,current->Name,#Name);
#define XL6_DIRECT_DETAIL(Encoding,Type,Name) CHECK_##Encoding(old->Name,current->details->Name,#Name);
#define XL6_DIRECT_ARRAY(Encoding,Type,Name,Count) for(size_t p=0;p<Count;++p){CHECK_##Encoding(old->Name[p],current->details->Name[p],#Name);}
#define XL6_DIRECT_PARAM(Encoding,Type,Name,Member) for(size_t p=0;p<16;++p){if(p<old->parameterCount){CHECK_##Encoding(old->Name[p],current->parameters[p].Member,#Name);}else{CHECK_##Encoding(old->Name[p],0,#Name);}}
#include "src/ir/xl6_ir_direct_bridge_fields.inc"
#undef XL6_DIRECT_BASE
#undef XL6_DIRECT_DETAIL
#undef XL6_DIRECT_ARRAY
#undef XL6_DIRECT_PARAM
        size_t length=strlen(old->name);char* quoted=malloc(length+3);if(!quoted)return 2;
        quoted[0]='"';memcpy(quoted+1,old->name,length);quoted[length+1]='"';quoted[length+2]=0;
        if(xl6IrDirectBridgeContractSpelling(quoted)!=current)return 3;
        free(quoted);
    }
    if(xl6IrDirectBridgeContract(NULL)||xl6IrDirectBridgeContract("")||
       xl6IrDirectBridgeContract("not_a_native_contract")||
       xl6IrDirectBridgeContractSpelling("\\\"\\\"")||
       xl6IrDirectBridgeContractSpelling("\\\"not_a_native_contract\\\""))return 4;
    size_t compact=sizeof(xl6IrDirectBridgeContracts)+sizeof(xl6IrDirectBridgeParameterPool)+sizeof(xl6IrDirectBridgeDetailsPool);
    if(compact>=sizeof(xl6IrDirectBridgeDeclarations)/2)return 5;
    printf("{\\\"contracts\\\":%zu,\\\"declarationBytes\\\":%zu,\\\"compactBytes\\\":%zu,\\\"headerSize\\\":%zu,\\\"parameterSize\\\":%zu,\\\"detailSize\\\":%zu}\\n",
        count,sizeof(xl6IrDirectBridgeDeclarations),compact,sizeof(XL6_IrDirectBridgeContract),sizeof(XL6_IrDirectBridgeParameter),sizeof(XL6_IrDirectBridgeDetails));
    volatile unsigned long long checksums[2]={0,0};
    double seconds[2]={0,0};
    for(int round=0;round<3;++round)for(int phase=0;phase<2;++phase){
        unsigned state=7;clock_t begin=clock();
        for(size_t i=0;i<50000;++i){
            state=state*1664525u+1013904223u;
            const char* name=xl6IrDirectBridgeDeclarations[state%count].name;
            checksums[phase]+=phase?xl6IrDirectBridgeContract(name)->parameterCount:legacy(name)->parameterCount;
        }
        seconds[phase]+=(double)(clock()-begin)/CLOCKS_PER_SEC;
    }
    if(checksums[0]!=checksums[1])return 6;
    fprintf(stderr,"{\\\"lookupQueriesPerVariant\\\":150000,\\\"legacySeconds\\\":%.6f,\\\"compactSeconds\\\":%.6f}\\n",seconds[0],seconds[1]);
    return 0;
}
'''
    source = out / "compare.c"
    source.write_text(prefix, encoding="utf-8", newline="\n")
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(root),
             "-I" + str(root / "src"), "-I" + str(root / "src/ir"),
             "-I" + str(root / "release"), "-I" + str(root / "release/include")]
    report = dict(complete=False, stats=stats, runs=[])

    def run(name, catalog, valid):
        binary = out / (name + (".exe" if os.name == "nt" else ""))
        command = [args.cc, *flags, '-DCATALOG_FILE="' + catalog.as_posix() + '"',
                   str(source), "-o", str(binary)]
        subprocess.run(command, cwd=root, check=True)
        start = time.monotonic()
        result = subprocess.run([str(binary)], capture_output=True, cwd=root, timeout=30)
        (out / (name + ".log")).write_bytes(result.stdout + result.stderr)
        report["runs"].append(dict(name=name, exit=result.returncode, seconds=time.monotonic()-start))
        if (result.returncode == 0) != valid:
            raise RuntimeError(name + " did not preserve/reject the compiled contract as expected")
        if valid:
            report["storage"] = json.loads(result.stdout)
            report["lookupBenchmark"] = json.loads(result.stderr)

    run("equivalence", data, True)
    if not args.skip_mutations:
        for name, marker in [("wrong-parameter", "xl6IrDirectBridgeParameterPool[]={\n"),
                             ("wrong-result", "xl6IrDirectBridgeDetailsPool[]={\n")]:
            start = expected.index(marker) + len(marker)
            begin = expected.index("{", start) + 1
            end = expected.index(",", begin)
            mutant = expected[:begin] + str(int(expected[begin:end]) ^ 1) + expected[end:]
            path = out / (name + ".inc")
            path.write_text(mutant, encoding="utf-8", newline="\n")
            run(name, path, False)
    report["complete"] = True
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
