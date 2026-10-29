#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const struct { const char* name; XL6_ArtifactRuntimeId owner; } rows[]={
        {"xl6CodecHexViewV1",XL6_ARTIFACT_CODEC_HEX},
        {"xl6CodecBase64ViewV1",XL6_ARTIFACT_CODEC_BASE64},
        {"xl6CodecPercentViewV1",XL6_ARTIFACT_CODEC_PERCENT},
        {"xl6TemplateCheckV1",XL6_ARTIFACT_TEMPLATE_CORE},
        {"xl6TemplateCompileConfigV1",XL6_ARTIFACT_TEMPLATE_CORE},
        {"xl6TemplateTakeV1",XL6_ARTIFACT_TEMPLATE_CORE},
        {"xl6TemplateDataV1",XL6_ARTIFACT_TEMPLATE_DATA},
        {"xl6TemplateInvokeV1",XL6_ARTIFACT_TEMPLATE_RENDER},
        {"xl6TemplateTokenCheckV1",XL6_ARTIFACT_TEMPLATE_RENDER},
        {"xl6TemplateWriteOutputV1",XL6_ARTIFACT_TEMPLATE_RENDER}
    };
    for(size_t i=0;i<sizeof(rows)/sizeof(rows[0]);++i) {
        assert(xl6ArtifactRuntimeSymbolOwner(rows[i].name)==rows[i].owner);
        char near[128]; assert(strlen(rows[i].name)+7<sizeof(near));
        snprintf(near,sizeof(near),"%sAlmost",rows[i].name);
        assert(xl6ArtifactRuntimeSymbolOwner(near)==XL6_ARTIFACT_RUNTIME_COUNT);
        XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(rows[i].owner);
        XL6_ArtifactRuntimePlan plan;
        assert(xl6ArtifactRuntimeClosePlan(&roots,&plan));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,rows[i].owner));
    }
    puts("10 exact internal symbols and 10 near-miss refusals; component closures verified");
    return 0;
}
