#define XRT_MODULE_ALL
#include "../include/terraform_state.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* 与 basic.xl 查询同一份原始 state；输入和结果均不落盘。 */
int main(void)
{
    const char* text = "{\"version\":4,\"serial\":7,\"lineage\":\"demo\","
        "\"outputs\":{\"answer\":{\"value\":42}},\"resources\":["
        "{\"type\":\"aws_instance\",\"name\":\"web\",\"instances\":[{\"attributes\":{\"id\":\"i-demo\"}}]},"
        "{\"module\":\"module.site\",\"mode\":\"data\",\"type\":\"aws_ami\",\"name\":\"ubuntu\","
        "\"instances\":[{\"attributes\":{\"id\":\"ami-demo\"}}]}]}";
    xvalue* state = xtf_parse(text);
    xvalue* addresses;
    xvalue* attributes;
    xvalue* image_attributes;
    xvalue* answer;
    char* lineage;
    xstrview id;
    xstrview source;
    xstrview image_id;
    int64 value;

    if (state == NULL) return 1;
    addresses = xtf_addresses(state);
    attributes = xtf_attributes(state, "aws_instance.web", 0);
    image_attributes = xtf_attributes(state, "module.site.data.aws_ami.ubuntu", 0);
    answer = xtf_output(state, "answer");
    lineage = xtf_lineage(state);
    if (addresses == NULL || attributes == NULL || image_attributes == NULL ||
        answer == NULL || lineage == NULL || xrtValueCount(addresses) != 2u ||
        !xrtValueGetString(xrtValueObjectGet(attributes, XRT_STR_LITERAL("id")), &id) ||
        !xrtValueGetString(xrtValueArrayGet(addresses, 1u), &source) ||
        !xrtValueGetString(xrtValueObjectGet(image_attributes, XRT_STR_LITERAL("id")), &image_id) ||
        !xrtValueGetInt(xrtValueObjectGet(answer, XRT_STR_LITERAL("value")), &value)) {
        xrtFree(lineage);
        xrtValueRelease(answer);
        xrtValueRelease(attributes);
        xrtValueRelease(image_attributes);
        xrtValueRelease(addresses);
        xrtValueRelease(state);
        return 1;
    }
    printf("version=%" PRId64 "\nlineage=%s\nresources=%zu\nid=%.*s\nsource=%.*s\nimage=%.*s\nanswer=%lld\n",
        xtf_version(state), lineage, xrtValueCount(addresses), (int)id.Size, id.Data,
        (int)source.Size, source.Data, (int)image_id.Size, image_id.Data,
        (long long)value);
    xrtFree(lineage);
    xrtValueRelease(answer);
    xrtValueRelease(attributes);
    xrtValueRelease(image_attributes);
    xrtValueRelease(addresses);
    xrtValueRelease(state);
    return 0;
}

/* TCC 单文件范例编入实现；正式包仍按清单单独编译。 */
#ifdef __TINYC__
#include "../terraform_state_native.c"
#endif
