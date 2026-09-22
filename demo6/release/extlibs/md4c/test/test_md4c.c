#define XRT_MODULE_ALL
#include <stdio.h>
#include <string.h>

int xmd4c_available(void);
const char* xmd4c_html(const char* markdown);
const char* xmd4c_html_flags(const char* markdown, int parserFlags, int rendererFlags);
const char* xmd4c_file_html(const char* path, int parserFlags, int rendererFlags);
const char* xmd4c_last_error(void);
int xmd4c_commonmark_flags(void);
int xmd4c_github_flags(void);
int xmd4c_no_html_flag(void);

int main(void)
{
    const char* html;

    if ( !xmd4c_available() ) return 1;
    html = xmd4c_html("# xlang\n\n**runtime**");
    if ( strstr(html, "<h1>xlang</h1>") == NULL || strstr(html, "<strong>runtime</strong>") == NULL ) return 2;
    html = xmd4c_html_flags("<script>x</script>", xmd4c_github_flags() | xmd4c_no_html_flag(), 0);
    if ( strstr(html, "<script>") != NULL ) return 3;
    if ( xmd4c_file_html("release/extlibs/md4c/test/missing.md", xmd4c_commonmark_flags(), 0)[0] != '\0' ) return 4;
    if ( strcmp(xmd4c_last_error(), "failed to read file") != 0 ) return 5;
    if ( strstr(xmd4c_html("plain"), "<p>plain</p>") == NULL || xmd4c_last_error()[0] != '\0' ) return 6;

    puts("md4c c smoke ok");
    return 0;
}

#include "../md4c_native.c"
#include "../third_party/md4c/md4c.c"
#include "../third_party/md4c/md4c-html.c"
#include "../third_party/md4c/entity.c"
