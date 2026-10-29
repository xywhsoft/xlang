#define XRT_MODULE_TEMPLATE_FILE
#define XRT_MODULE_TEMPLATE_CONTROL
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FILE_TEXT
#define XRT_MODULE_DIR
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出与 xlang 示例一致的检查结果，便于发布门禁直接比较。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 比较拥有型渲染结果和预期文本。 */
static int text_eq(const char* text, const char* expected)
{
    return (text != NULL) && (expected != NULL) && (strcmp(text, expected) == 0);
}



/* 使用可选的独立开闭分隔符编译并渲染一段模板。 */
static char* render_text(
    const char* source,
    const xvalue* data,
    const char* open,
    const char* close)
{
    xtemplateconfig config;
    xtemplate* tpl;
    char* out;

    xrtTemplateConfigInit(&config);
    if ( open != NULL ) {
        config.Open = xrtStrView(open);
    }
    if ( close != NULL ) {
        config.Close = xrtStrView(close);
    }
    tpl = xrtTemplateCompileConfig(xrtStrView(source), &config);
    if ( tpl == NULL ) {
        return NULL;
    }
    out = xrtTemplateRender(tpl, data, NULL);
    xrtTemplateRelease(tpl);
    return out;
}



/* 构建与 xlang 示例相同的对象和对象数组。 */
static xvalue* make_model(void)
{
    xvalue* data = xrtValueObject();
    xvalue* users = xrtValueArray();
    xvalue* user;

    if ( (data == NULL) || (users == NULL) ||
         !xrtValueObjectSetNew(
            data,
            XRT_STR_LITERAL("title"),
            xrtValueString(XRT_STR_LITERAL("xlang"))
         ) ||
         !xrtValueObjectSetNew(
            data,
            XRT_STR_LITERAL("show"),
            xrtValueBool(true)
         ) ) {
        xrtValueRelease(users);
        xrtValueRelease(data);
        return NULL;
    }

    user = xrtValueObject();
    if ( (user == NULL) ||
         !xrtValueObjectSetNew(
            user,
            XRT_STR_LITERAL("name"),
            xrtValueString(XRT_STR_LITERAL("Ada"))
         ) ||
         !xrtValueArrayAppendTake(users, &user) ) {
        xrtValueRelease(user);
        xrtValueRelease(users);
        xrtValueRelease(data);
        return NULL;
    }

    user = xrtValueObject();
    if ( (user == NULL) ||
         !xrtValueObjectSetNew(
            user,
            XRT_STR_LITERAL("name"),
            xrtValueString(XRT_STR_LITERAL("Lin"))
         ) ||
         !xrtValueArrayAppendTake(users, &user) ||
         !xrtValueObjectSetTake(
            data,
            XRT_STR_LITERAL("users"),
            &users
         ) ) {
        xrtValueRelease(user);
        xrtValueRelease(users);
        xrtValueRelease(data);
        return NULL;
    }
    return data;
}



/* 覆盖模板编译、稳定视图、自定义分隔符和文件编译入口。 */
int main(void)
{
    const char* filePath = "examples_out/stdlib_template_example/page.xte";
    xvalue* data;
    xtemplate* compiled;
    xtemplate* fileTemplate;
    xtemplatenodeview node;
    char* out;
    char* out2;

    data = make_model();
    compiled = xrtTemplateCompile(XRT_STR_LITERAL(
        "<h1>{$title}</h1>{#if:show}Y{#else}N{#end}:"
        "{#foreach:users}{$name};{#end}"
    ));
    if ( (data == NULL) || (compiled == NULL) ) {
        return 1;
    }

    out = xrtTemplateRender(compiled, data, NULL);
    require_line("basic", text_eq(out, "<h1>xlang</h1>Y:Ada;Lin;"));
    require_line(
        "inspect",
        (xrtTemplateNodeCount(compiled) > 0u) &&
        xrtTemplateNode(compiled, 0u, &node) &&
        (node.Type > 0)
    );
    xrtFree(out);
    xrtTemplateRelease(compiled);

    out = render_text("Hello {$title}", data, NULL, NULL);
    require_line("render", text_eq(out, "Hello xlang"));
    xrtFree(out);

    out = render_text("{{$title}}", data, "{{", "}}");
    out2 = render_text("<$title>", data, "<", ">");
    require_line("bracket", text_eq(out, "xlang") && text_eq(out2, "xlang"));
    xrtFree(out);
    xrtFree(out2);

    if ( !xrtDirCreateAll("examples_out/stdlib_template_example") ||
         !xrtFileWriteText(
            filePath,
            XRT_STR_LITERAL("File {$title}"),
            XENCODING_UTF8,
            XUTF_STRICT,
            false
         ) ) {
        fprintf(stderr, "file=fail\n");
        return 1;
    }
    fileTemplate = xrtTemplateCompileFile(filePath);
    out = xrtTemplateRender(fileTemplate, data, NULL);
    require_line("file", text_eq(out, "File xlang"));
    xrtFree(out);
    xrtTemplateRelease(fileTemplate);
    xrtFileDelete(filePath);
    xrtDirRemove("examples_out/stdlib_template_example");

    xrtValueRelease(data);
    return 0;
}
