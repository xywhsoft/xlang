/* HCL C smoke: same syntax-aware restricted parser as the xlang package. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

cstr xhcl_get(cstr sHcl, cstr sKey);
cstr xhcl_get_raw(cstr sHcl, cstr sKey);
long long xhcl_count_blocks(cstr sHcl, cstr sType);
cstr xhcl_block_label_at(cstr sHcl, cstr sType, long long iIndex, long long iLabel);
long long xhcl_block_label_count(cstr sHcl, cstr sType, long long iIndex);
long long xhcl_count_attrs(cstr sHcl);
bool xhcl_validate(cstr sHcl);

static int fail(cstr sMessage)
{
    fprintf(stderr, "hcl C smoke failed: %s\n", sMessage);
    return 1;
}

int main(void)
{
    cstr sHcl = "# comment\nresource \"aws_instance\" \"web\" {\n"
        "  name = \"web = primary\"\n  tags = {\n    role = \"api\"\n  }\n"
        "  nested { enabled = false }\n}\n";

    if (!xhcl_validate(sHcl) || xhcl_count_blocks(sHcl, "resource") != 1 ||
        xhcl_count_blocks(sHcl, "nested") != 1 ||
        xhcl_block_label_count(sHcl, "resource", 0) != 2 ||
        strcmp(xhcl_block_label_at(sHcl, "resource", 0, 1), "web") != 0 ||
        strcmp(xhcl_get(sHcl, "name"), "web = primary") != 0 ||
        strcmp(xhcl_get_raw(sHcl, "tags"), "{\n    role = \"api\"\n  }") != 0 ||
        xhcl_count_attrs(sHcl) != 3 ||
        xhcl_validate("resource \"bad\" { name = \"unterminated }") ) {
        return fail("syntax-aware queries");
    }
    puts("hcl C smoke: PASS");
    return 0;
}
