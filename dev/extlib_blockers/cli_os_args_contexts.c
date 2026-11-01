/* 两个独立上下文使用同一个DLL的XRT；不编译第二份运行库。 */
#include "xl.h"
#include "xrt_decl.h"
#include <stdio.h>
#include <string.h>

static int processCount;
static char** processArguments;
static int host_count(void) { return processCount; }
static const char* host_argument(int index)
{
    return index >= 0 && index < processCount ? processArguments[index] : NULL;
}

static const char source[] =
    "extern \"cli_host_argc\" func hostCount() as c_int\n"
    "extern \"cli_host_arg\" func hostArgument(c_int index) as string\n"
    "func main(...) as int\n"
    "    var actual = os.args()\n"
    "    if type(actual) != array then return 80 end\n"
    "    println(\"context args count: os=\", actual.length, \" main=\", arg.length)\n"
    "    if actual.length != int(hostCount()) then return 81 end\n"
    "    for index in range(actual.length)\n"
    "        if actual[index] != hostArgument(c_int(index)) then return 82 end\n"
    "    end\n"
    "    if arg[0] == \"context-a\" then\n"
    "        if arg.length != 4 or arg[1] != \"alpha\" or arg[2] != \"\" or arg[3] != \"中文\" then return 83 end\n"
    "    else\n"
    "        if arg.length != 2 or arg[0] != \"context-b\" or arg[1] != \"two words\" then return 84 end\n"
    "    end\n"
    "    return 0\n"
    "end\n";

static xvalue* arguments(const char* const* text, size_t count)
{
    xvalue* result = xrtValueArray();
    if (!result) return NULL;
    for (size_t i = 0; i < count; ++i) {
        xvalue* item = xrtValueString((xstrview){text[i], strlen(text[i])});
        if (!item || !xrtValueArrayAppendTake(result, &item)) {
            xrtValueRelease(item);
            xrtValueRelease(result);
            return NULL;
        }
    }
    return result;
}

static int run(XL_Context* context, xvalue* args, const char* label, size_t count)
{
    int executed = xlRunScriptStringArg(context, source, args);
    int exitCode = xlGetLastExitCode(context);
    int ok = executed && exitCode == 0 && xrtValueCount(args) == count;
    printf("%s: executed=%d exit=%d args=%zu\n", label, executed, exitCode, xrtValueCount(args));
    if (!executed) fprintf(stderr, "%s\n", xlGetLastError(context));
    return ok;
}

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    processCount = argc; processArguments = argv;
    XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = argv[1]; options.tempDir = argv[2];
    XL_Context* first = xlCreateContext(&options);
    XL_Context* second = xlCreateContext(&options);
    int bound = first && second &&
        xlRegisterCFunction(first, "cli_host_argc", (void*)host_count) &&
        xlRegisterCFunction(first, "cli_host_arg", (void*)host_argument) &&
        xlRegisterCFunction(second, "cli_host_argc", (void*)host_count) &&
        xlRegisterCFunction(second, "cli_host_arg", (void*)host_argument);
    const char* a[] = {"context-a", "alpha", "", "中文"};
    const char* b[] = {"context-b", "two words"};
    xvalue* firstArgs = bound ? arguments(a, 4) : NULL;
    xvalue* secondArgs = bound ? arguments(b, 2) : NULL;
    int ok = firstArgs && secondArgs;
    if (ok) {
        int a1 = run(first, firstArgs, "A first", 4);
        int b1 = run(second, secondArgs, "B first", 2);
        int a2 = run(first, firstArgs, "A repeat", 4);
        ok = a1 && b1 && a2;
    }
    xrtValueRelease(firstArgs); xrtValueRelease(secondArgs);
    if (second) xlDestroyContext(second);
    if (first) xlDestroyContext(first);
    XL_RuntimeDrainResult drained = xlRuntimeDrainRetiringChecked(-1);
    if (!drained.ready || !drained.ok) ok = 0;
    return ok ? 0 : 1;
}
