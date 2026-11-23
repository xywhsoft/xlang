#include "foreign_xui_flow_workflow_coverage.h"
#include <assert.h>
#include <string.h>

static bool equals_field(const xvalue* row,const char* key,const char* expected)
{
    xstrview text={0};
    return xrtValueGetString(xrtValueObjectGet(row,xrtStrView(key)),&text) &&
        text.Size==strlen(expected) && memcmp(text.Data,expected,text.Size)==0;
}

int flowWorkflowCompareDiagnostics(xui_workflow workflow,const char* type,xvalue* config,xvalue* report)
{
    xui_workflow_config_diagnostic_t rows[4]={0};
    int count=0;
    assert(xuiWorkflowValidateConfigEx(workflow,type,config,rows,1,&count)==XUI_OK);
    assert(count==2 && rows[0].sCode && !rows[1].sCode);
    assert(xuiWorkflowValidateConfigEx(workflow,type,config,rows,4,&count)==XUI_OK);
    assert(count==2 && xrtValueCount(report)==(size_t)count);
    for(int i=0;i<count;++i){
        const xvalue* row=xrtValueArrayGet(report,(size_t)i);
        assert(equals_field(row,"path",rows[i].sPath));
        assert(equals_field(row,"code",rows[i].sCode));
        assert(equals_field(row,"message",rows[i].sMessage));
    }
    return count;
}
