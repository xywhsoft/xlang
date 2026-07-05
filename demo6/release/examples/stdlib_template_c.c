#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int text_eq(const char* text, const char* expected)
{
    return text != NULL && expected != NULL && strcmp(text, expected) == 0;
}

static char* render_text(const char* source, xvalue data, const char* bracket)
{
    xtetemplate tpl = xteParse(source, 0u, bracket);
    char* out = NULL;

    if ( tpl == NULL ) {
        return NULL;
    }
    out = xteMake(tpl, data, NULL, NULL, NULL);
    xteDestroyTemplate(tpl);
    return out;
}

static xvalue make_model(void)
{
    xvalue data = xvoCreateTable();
    xvalue users = xvoCreateArray();
    xvalue user = NULL;

    xvoTableSetText(data, "title", 0, "xlang", 0, FALSE);
    xvoTableSetBool(data, "show", 0, TRUE);

    user = xvoCreateTable();
    xvoTableSetText(user, "name", 0, "Ada", 0, FALSE);
    xvoArrayAppendValue(users, user, TRUE);

    user = xvoCreateTable();
    xvoTableSetText(user, "name", 0, "Lin", 0, FALSE);
    xvoArrayAppendValue(users, user, TRUE);

    xvoTableSetValue(data, "users", 0, users, TRUE);
    return data;
}

int main(void)
{
    xvalue data = NULL;
    xtetemplate compiled = NULL;
    char* out = NULL;
    char* out2 = NULL;
    char* out3 = NULL;
    const char* filePath = "examples_out/stdlib_template_example/page.xte";

    xrtInit();

    data = make_model();
    compiled = xteParse("<h1>{$title}</h1>{#if:show}Y{#else}N{#end}:{#foreach:users}{$name};{#end}", 0u, NULL);
    out = xteMake(compiled, data, NULL, NULL, NULL);
    require_line("basic", text_eq(out, "<h1>xlang</h1>Y:Ada;Lin;"));
    require_line("stats",
        xteTemplateGetNodeCount(compiled) > 0u &&
        xteTemplateGetExprCount(compiled) > 0u &&
        xteTemplateGetArgCount(compiled) > 0u &&
        xteTemplateGetStringPoolSize(compiled) > 0u);
    xrtFree(out);
    xteDestroyTemplate(compiled);

    out = render_text("Hello {$title}", data, NULL);
    require_line("render", text_eq(out, "Hello xlang"));
    xrtFree(out);

    out = render_text("{{$title}}", data, "{{}}");
    out2 = render_text("<$title>", data, "<>");
    require_line("bracket", text_eq(out, "xlang") && text_eq(out2, "xlang"));
    xrtFree(out);
    xrtFree(out2);

    if ( !xrtDirCreateAll("examples_out/stdlib_template_example") ||
         !xrtFileWriteAll((str)filePath, (str)"File {$title}", 0u, XRT_CP_UTF8) ) {
        fprintf(stderr, "file=fail\n");
        exit(1);
    }
    out3 = xrtFileReadAll((str)filePath, XRT_CP_UTF8, NULL);
    out = render_text(out3, data, NULL);
    require_line("file", text_eq(out, "File xlang"));
    xrtFree(out);
    xrtFree(out3);
    xrtFileDelete((str)filePath);
    xrtDirDelete((str)"examples_out/stdlib_template_example");

    xvoUnref(data);
    xrtUnit();
    return 0;
}
