#include "test/cases/foreign_xui_document_browser.h"

int fixtureMeasureDocumentMath(xui_doc_web_provider provider,xui_document_snapshot snapshot,uint64_t node)
{
    xui_vec2_t size = {0};
    float baseline = 0.0f;
    return xuiDocumentWebObjectMeasure(snapshot,node,360.0f,1.0f,&size,&baseline,provider);
}
