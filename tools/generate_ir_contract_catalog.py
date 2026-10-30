"""Compile declarative native contracts into shared immutable C records.

The C compiler evaluates the declarations and enum vocabulary. Python only
interns exact records; it does not interpret C initializers or language ABIs.
"""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess


def schema(root):
    fields = []
    text = (root / "src/ir/xl6_ir_direct_bridge_fields.inc").read_text(encoding="utf-8")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    for line in text.splitlines():
        if not line.strip():
            continue
        match = re.fullmatch(r"XL6_DIRECT_(BASE|PARAM|DETAIL|ARRAY)\((.*)\)", line.strip())
        if not match:
            raise ValueError("unrecognized contract schema: " + line)
        role = match[1]
        parts = [p.strip() for p in match[2].split(",")]
        if len(parts) != (4 if role in ("PARAM", "ARRAY") else 3) or parts[0] not in ("INT", "STRING", "ENUM"):
            raise ValueError("invalid contract field: " + line)
        fields.append(dict(role=role, encoding=parts[0], type=parts[1], name=parts[2],
                           member=parts[3] if role == "PARAM" else None,
                           count=int(parts[3]) if role == "ARRAY" else 16 if role == "PARAM" else None))
    if [f["name"] for f in fields if f["role"] == "BASE"] != ["name", "kind", "result", "parameterCount"]:
        raise ValueError("new base fields require an explicit compact header mapping")
    if len({f["name"] for f in fields}) != len(fields):
        raise ValueError("duplicate contract field")
    return fields


def compile_declarations(root, directory, cc, fields):
    directory.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-O2", "-I" + str(root), "-I" + str(root / "src"),
             "-I" + str(root / "src/ir"), "-I" + str(root / "release"),
             "-I" + str(root / "release/include")]
    prefix = '''#include "src/ir/xl6_ir.h"
#include "src/ir/xl6_ir_direct_bridge_schema.h"
#include "src/ir/xl6_ir_direct_bridge_enums.inc"
#include "src/ir/xl6_ir_direct_bridge_declarations.inc"
'''
    source = directory / "declarations.c"
    source.write_text(prefix, encoding="utf-8", newline="\n")
    expanded = subprocess.run([cc, *flags, "-E", "-P", str(source)], cwd=root,
                              capture_output=True, check=True).stdout.decode("utf-8")
    enums = sorted(set(re.findall(r"static\s+const\s+XL6_IrNumericEnumContract\s+(\w+)\s*=", expanded)))
    if not enums:
        raise ValueError("numeric enum vocabulary was not compiled")
    code = prefix + '''#include <stdio.h>
#include <stdlib.h>
static void string(const char* text) {
    if(!text){fputs("null",stdout);return;}
    putchar('"');
    for(const unsigned char* p=(const unsigned char*)text;*p;++p){
        if(*p=='"'||*p=='\\\\'){putchar('\\\\');putchar(*p);}
        else if(*p<32)printf("\\\\u%04x",*p);
        else putchar(*p);
    }
    putchar('"');
}
static void numericEnum(const XL6_IrNumericEnumContract* value) {
    if(!value){fputs("null",stdout);return;}
'''
    code += "".join(f'    if(value==&{name}){{string("{name}");return;}}\n' for name in enums)
    code += '    fputs("unknown numeric enum contract\\n",stderr);exit(2);\n}\n'
    code += 'int main(void){putchar(\'[\');\n'
    code += 'for(size_t i=0;i<sizeof(xl6IrDirectBridgeDeclarations)/sizeof(xl6IrDirectBridgeDeclarations[0]);++i){\n'
    code += 'const XL6_IrDirectBridgeDeclaration* row=&xl6IrDirectBridgeDeclarations[i];if(i)putchar(\',\');putchar(\'{\');\n'
    for index, field in enumerate(fields):
        name = field["name"]
        code += ("putchar(',');" if index else "") + f'string("{name}");putchar(\':\');'
        if field["count"]:
            code += f"putchar('[');for(size_t j=0;j<{field['count']};++j){{if(j)putchar(',');"
        value = "row->" + name + ("[j]" if field["count"] else "")
        code += f'printf("%lld",(long long){value});' if field["encoding"] == "INT" else (
            f"string({value});" if field["encoding"] == "STRING" else f"numericEnum({value});")
        if field["count"]:
            code += "}putchar(']');"
        code += "\n"
    code += "putchar('}');}puts(\"]\");return ferror(stdout)?1:0;}\n"
    source.write_text(code, encoding="utf-8", newline="\n")
    binary = directory / ("declarations.exe" if os.name == "nt" else "declarations")
    subprocess.run([cc, *flags, str(source), "-o", str(binary)], cwd=root, check=True)
    result = subprocess.run([str(binary)], cwd=root, capture_output=True, check=True)
    (directory / "declarations.json").write_bytes(result.stdout)
    return json.loads(result.stdout)


def literal(value, encoding):
    if encoding == "INT":
        return str(value)
    if value is None:
        return "NULL"
    if encoding == "ENUM":
        if not re.fullmatch(r"[A-Za-z_]\w*", value):
            raise ValueError("invalid enum identifier")
        return "&" + value
    text = '"'
    for byte in value.encode("utf-8"):
        text += chr(byte) if 32 <= byte < 127 and byte not in (34, 92) else "\\%03o" % byte
    return text + '"'


def generate(fields, declarations):
    params = [f for f in fields if f["role"] == "PARAM"]
    details = [f for f in fields if f["role"] in ("DETAIL", "ARRAY")]
    pool, spans, records, record_ids, rows, names = [], {}, [], {}, [], set()
    for row in declarations:
        count, name = row["parameterCount"], row["name"]
        if not name or name in names or count < 0 or count > 16:
            raise ValueError("invalid or duplicate native contract: " + str(name))
        names.add(name)
        for field in params:
            tail = row[field["name"]][count:]
            if any(value not in (None, 0) for value in tail):
                raise ValueError("contract has parameter metadata beyond arity: " + name)
        values = tuple(tuple(row[f["name"]][i] for f in params) for i in range(count))
        if values not in spans:
            spans[values] = len(pool)
            pool.extend(values)
        key = tuple(tuple(row[f["name"]]) if f["role"] == "ARRAY" else row[f["name"]] for f in details)
        if key not in record_ids:
            record_ids[key] = len(records)
            records.append(key)
        rows.append((name, row["kind"], row["result"], count, spans[values], record_ids[key]))
    lines = ['/* Generated by tools/generate_ir_contract_catalog.py; do not edit. */',
             'static const XL6_IrDirectBridgeParameter xl6IrDirectBridgeParameterPool[]={']
    for values in pool:
        lines.append("    {" + ",".join(literal(v, f["encoding"]) for v, f in zip(values, params)) + "},")
    if not pool:
        lines.append("    {0},")
    lines += ["};", "static const XL6_IrDirectBridgeDetails xl6IrDirectBridgeDetailsPool[]={"]
    for record in records:
        values = []
        for value, field in zip(record, details):
            values.append("{" + ",".join(literal(v, field["encoding"]) for v in value) + "}"
                          if field["role"] == "ARRAY" else literal(value, field["encoding"]))
        lines.append("    {" + ",".join(values) + "},")
    lines += ["};", "static const XL6_IrDirectBridgeContract xl6IrDirectBridgeContracts[]={"]
    for name, kind, result, count, start, detail in sorted(rows):
        length = len(name.encode("utf-8"))
        if length > 0xFFFFFFFF:
            raise ValueError("native linkage name exceeds storage range")
        lines.append(f"    {{{literal(name,'STRING')},{kind},{result},{count},{length},"
                     f"xl6IrDirectBridgeParameterPool+{start},xl6IrDirectBridgeDetailsPool+{detail}}},")
    lines += ["};", ""]
    return "\n".join(lines), dict(contracts=len(rows), parameters=sum(row[3] for row in rows),
                                  storedParameters=len(pool), detailRecords=len(records))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    directory = (args.directory or root / "build/tmp/ir-contract-catalog").resolve()
    fields = schema(root)
    declarations = compile_declarations(root, directory, args.cc, fields)
    text, stats = generate(fields, declarations)
    destination = root / "src/ir/xl6_ir_direct_bridge_data.inc"
    if args.check:
        if not destination.is_file() or destination.read_text(encoding="utf-8") != text:
            raise SystemExit("native contract catalog is stale; regenerate it")
    else:
        destination.write_text(text, encoding="utf-8", newline="\n")
    (directory / "summary.json").write_text(json.dumps(stats, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
