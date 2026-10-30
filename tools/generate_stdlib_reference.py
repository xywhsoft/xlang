"""Render the public function/constant reference from the compiled registry.

The tiny producer only prints immutable declaration data; it never initializes
the language runtime or invokes a standard-library operation.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

PRODUCER = r'''
#include "src/stdlib/xl6_stdlib_data.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(void) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    for (size_t i=0; i<xl6StdlibFuncCount; ++i) {
        const XL6_StdlibFunc* f=&xl6StdlibFuncs[i];
        if (f->ns[0]=='_') continue;
        printf("F\t%s\t%s\t%s\t%s\t%zu\n", f->ns, f->name,
            f->internalName, f->returnType ? f->returnType : "", f->paramCount);
        for (size_t j=0; j<f->paramCount; ++j)
            printf("P\t%s\t%s\n", f->params[j].type, f->params[j].name);
        const char* body=f->sourceBody ? f->sourceBody : "";
        printf("B\t%zu\n", strlen(body));
        fwrite(body, 1, strlen(body), stdout); putchar('\n');
    }
    for (size_t i=0; i<xl6StdlibConstCount; ++i) {
        const XL6_StdlibConst* c=&xl6StdlibConsts[i];
        if (c->ns[0]!='_') printf("C\t%s\t%s\t%s\n", c->ns, c->name, c->value);
    }
    return ferror(stdout) ? 1 : 0;
}
'''


def registry(root, cc):
    import io
    with tempfile.TemporaryDirectory(prefix="xl-stdlib-reference-") as temporary:
        temporary = Path(temporary)
        source = temporary / "registry.c"
        binary = temporary / ("registry.exe" if os.name == "nt" else "registry")
        source.write_text(PRODUCER, encoding="utf-8")
        subprocess.run([cc, "-std=c11", "-I.", "-Irelease", "-Isrc", str(source),
                        "src/stdlib/xl6_stdlib_data.c", "-o", str(binary)],
                       cwd=root, check=True)
        stream = io.BytesIO(subprocess.check_output([str(binary)]))
    functions, constants = [], []
    while line := stream.readline():
        fields = line.decode("utf-8").rstrip("\n").split("\t")
        if fields[0] == "C":
            constants.append(fields[1:])
            continue
        if fields[0] != "F":
            raise ValueError("invalid declaration record")
        ns, name, internal, result, count = fields[1:]
        parameters = [stream.readline().decode("utf-8").rstrip("\n").split("\t")[1:]
                      for _ in range(int(count))]
        size = int(stream.readline().split(b"\t")[1])
        body = stream.read(size).decode("utf-8")
        if stream.read(1) != b"\n":
            raise ValueError("truncated declaration record")
        functions.append((ns, name, internal, result, parameters, body))
    return functions, constants


def render(functions, constants):
    rows = {}
    for ns, name, internal, result, parameters, body in functions:
        public = f"{ns}.{name}" if ns else name
        declarations = re.findall(r"(?m)^.*?\bfunc " + re.escape(internal) +
                                  r"(?P<tail>(?:<[^\n]+?>)?\([^\n]*)$", body)
        if declarations:
            signatures = [public + tail for tail in declarations]
        else:
            signature = public + "(" + ", ".join(t + " " + n for t, n in parameters) + ")"
            signature += " as " + result if result else " [compiler signature]"
            signatures = [signature]
        rows.setdefault(ns, set()).update(signatures)
    lines = ["# 标准库函数与常量登记参考", "",
             "由 `tools/generate_stdlib_reference.py` 从实际编译后的声明登记表生成；不要手工编辑。",
             "这里只列命名空间函数和常量。类方法、错误语义、拥有权和效果合同请同时阅读各模块设计文档。",
             "`[compiler signature]` 表示登记项由编译器专用合同补齐，不表示可变参数或动态返回。",
             "示例或声明可编译不等于相关运行路径已经执行；本轮发布报告分别记录这些结果。", ""]
    for ns, signatures in sorted(rows.items()):
        lines += ["## " + (ns or "global"), "", "```text", *sorted(signatures), "```", ""]
    lines += ["## 常量", "", "| 名称 | 登记值 |", "| --- | --- |"]
    lines += [f"| `{ns}.{name}` | `{value}` |" for ns, name, value in sorted(constants)]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    target = args.root / "docs/stdlib-function-reference.md"
    text = render(*registry(args.root, args.cc))
    if args.check:
        if not target.is_file() or target.read_text(encoding="utf-8") != text:
            raise SystemExit("standard-library declaration reference is stale")
    else:
        target.write_text(text, encoding="utf-8")
    print("Standard-library declaration reference is current.")


if __name__ == "__main__":
    main()
