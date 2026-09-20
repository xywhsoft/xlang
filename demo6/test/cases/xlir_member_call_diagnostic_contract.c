#include "src/compile/xl6_compile_unit.h"
#include <stdio.h>
#include <string.h>

#define BASE "class Check\n func accept(int value) as bool\n return value >= 0\n end\nend\nCheck item = Check()\n"
static const struct { const char* name; const char* source; } invalid[] = {
    {"too-many", BASE "assert item.accept(1, 2)\n"},
    {"too-few", BASE "assert item.accept()\n"},
    {"wrong-type", BASE "assert item.accept(\"bad\")\n"},
    {"unknown-named", BASE "assert item.accept(other = 1)\n"},
    {"discarded-call", BASE "item.accept(1, 2)\n"},
    {"unknown-method", BASE "item.absent()\n"},
    {"inherited-arity", BASE "class Child extends Check\nend\nChild child = Child()\nassert child.accept(1, 2)\n"},
    {"generic-arity", "class Generic<T>\n func accept(T value) as bool\n return true\n end\nend\nGeneric<int> item = Generic::<int>()\nassert item.accept(1, 2)\n"},
    {"async-arity", "class AsyncCheck\n async func accept(int value) as bool\n return true\n end\nend\nAsyncCheck item = AsyncCheck()\nfuture<bool> result = item.accept(1, 2)\n"},
    {"non-callable-field", "class Field\n int value = 1\nend\nField item = Field()\nitem.value()\n"},
    {"native-tcp-arity", "func wrong(TcpStream stream)\n assert stream.send(\"value\", 8000)\nend\n"}
};

int main(void)
{
    unsigned rejected = 0;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        XL6_CompileUnit unit;
        char virtualPath[160];
        snprintf(virtualPath, sizeof(virtualPath), "__member_%s.xl", invalid[i].name);
        xl6CompileUnitInit(&unit);
        int accepted = xl6CompileUnitLoadSource(&unit, invalid[i].source, virtualPath, 1);
        int diagnostic = 0;
        for (size_t j = 0; j < unit.parse.diags.count; ++j) {
            const XL6_Diag* item = &unit.parse.diags.items[j];
            if (strstr(item->message, "no matching callable class member") &&
                item->line > 0 && item->column > 0) diagnostic = 1;
        }
        int complete = !accepted && !strcmp(unit.phase, "sem") &&
            unit.semChecked && !unit.irReady && unit.ir.nodeCount == 0 && diagnostic;
        printf("{\"Case\":\"%s\",\"Accepted\":%s,\"Phase\":\"%s\",\"IrNodes\":%zu,\"Diagnostic\":%s,\"Complete\":%s}\n",
            invalid[i].name, accepted ? "true" : "false", unit.phase, unit.ir.nodeCount,
            diagnostic ? "true" : "false", complete ? "true" : "false");
        if (!complete) xl6DiagPrintTextWithPhase(&unit.parse.diags, virtualPath, unit.phase);
        rejected += complete;
        xl6CompileUnitFree(&unit);
    }
    int complete = rejected == sizeof(invalid) / sizeof(invalid[0]);
    printf("{\"SemanticRejections\":%u,\"Cases\":%zu,\"Complete\":%s}\n",
        rejected, sizeof(invalid) / sizeof(invalid[0]), complete ? "true" : "false");
    return complete ? 0 : 1;
}
