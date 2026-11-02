/* 只经公开 DOM API 比较独立树模型；不读取生产节点、容量或文档引用计数。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ALL
#include "../include/xrt_xml_dom.h"
#include <stdio.h>
#include <string.h>

enum { NODES = 25, SEEDS = 32, STEPS = 128 };
typedef struct {
    xrtxmldomnode* nodes[NODES];
    int parent[NODES], children[NODES][NODES];
    unsigned count[NODES];
    cstr names[NODES];
} tree_model;
static unsigned checks, failures, matrix_cases, seeds_run, steps_run, oom_count, invalid_count;
static bool print_types;

static void require(bool ok, cstr label)
{
    ++checks;
    if (!ok) { if (failures < 12) fprintf(stderr, "XML DOM mutation: %s\n", label); ++failures; }
}

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static unsigned kind(unsigned node)
{
    return node == 0 ? XRT_XML_DOM_DOCUMENT_NODE : node <= 12 ? XRT_XML_DOM_ELEMENT_NODE :
        node <= 16 ? XRT_XML_DOM_TEXT_NODE : node <= 20 ? XRT_XML_DOM_COMMENT_NODE : XRT_XML_DOM_PROCESSING_INSTRUCTION_NODE;
}

static bool create(tree_model* model)
{
    memset(model, 0, sizeof(*model));
    for (unsigned i = 0; i < NODES; ++i) model->parent[i] = -1;
    model->nodes[0] = xrtXmlDomCreateDocument();
    if (model->nodes[0] == NULL) return false;
    for (unsigned i = 1; i < NODES; ++i) {
        char name[32]; snprintf(name, sizeof(name), "node_%u", i);
        switch (kind(i)) {
        case XRT_XML_DOM_ELEMENT_NODE: model->nodes[i] = xrtXmlDomCreateElement(model->nodes[0], name); break;
        case XRT_XML_DOM_TEXT_NODE: model->nodes[i] = xrtXmlDomCreateText(model->nodes[0], "你好"); break;
        case XRT_XML_DOM_COMMENT_NODE: model->nodes[i] = xrtXmlDomCreateComment(model->nodes[0], "note"); break;
        default: model->nodes[i] = xrtXmlDomCreateProcessingInstruction(model->nodes[0], name, "data"); break;
        }
        if (model->nodes[i] == NULL) return false;
    }
    for (unsigned i = 0; i < NODES; ++i) model->names[i] = xrtXmlDomNodeName(model->nodes[i]);
    return true;
}

static void destroy(tree_model* model)
{
    /* 先释放文档变量，再释放其余引用，节点必须让所属文档继续存活。 */
    for (unsigned i = 0; i < NODES; ++i) xrtXmlDomNodeRelease(model->nodes[i]);
}

static void verify(tree_model* model)
{
    int root = -1;
    for (unsigned i = 0; i < NODES; ++i) {
        xrtxmldomnode* parent = xrtXmlDomNodeParent(model->nodes[i]);
        require(parent == (model->parent[i] < 0 ? NULL : model->nodes[model->parent[i]]), "parent identity/model");
        xrtXmlDomNodeRelease(parent);
        require(xrtXmlDomNodeChildCount(model->nodes[i]) == model->count[i], "child count/model");
        require(xrtXmlDomNodeName(model->nodes[i]) == model->names[i], "borrowed name survives moves");
        for (unsigned j = 0; j < model->count[i]; ++j) {
            xrtxmldomnode* child = xrtXmlDomNodeChild(model->nodes[i], j);
            require(child == model->nodes[model->children[i][j]], "ordered child identity/model");
            xrtXmlDomNodeRelease(child);
        }
        xrtxmldomnode* outside = xrtXmlDomNodeChild(model->nodes[i], model->count[i]);
        require(outside == NULL, "out of range child is null");
        xrtXmlDomNodeRelease(outside);
        if (model->parent[i] == 0 && kind(i) == XRT_XML_DOM_ELEMENT_NODE) root = (int)i;
    }
    xrtxmldomnode* element = xrtXmlDomDocumentElement(model->nodes[0]);
    require(element == (root < 0 ? NULL : model->nodes[root]), "document element/model");
    xrtXmlDomNodeRelease(element);
}

static bool allowed(const tree_model* model, unsigned parent, unsigned child)
{
    if (kind(parent) != XRT_XML_DOM_DOCUMENT_NODE && kind(parent) != XRT_XML_DOM_ELEMENT_NODE) return false;
    if (kind(child) == XRT_XML_DOM_DOCUMENT_NODE ||
        (parent == 0 && kind(child) == XRT_XML_DOM_TEXT_NODE)) return false;
    for (int node = (int)parent; node >= 0; node = model->parent[node]) if (node == (int)child) return false;
    if (parent == 0 && kind(child) == XRT_XML_DOM_ELEMENT_NODE)
        for (unsigned i = 0; i < model->count[0]; ++i)
            if (model->children[0][i] != (int)child && kind((unsigned)model->children[0][i]) == XRT_XML_DOM_ELEMENT_NODE) return false;
    return true;
}

static void detach(tree_model* model, unsigned child)
{
    int parent = model->parent[child];
    if (parent < 0) return;
    unsigned i = 0;
    while (i < model->count[parent] && model->children[parent][i] != (int)child) ++i;
    if (i == model->count[parent]) { require(false, "reference child exists"); return; }
    for (unsigned j = i + 1; j < model->count[parent]; ++j) model->children[parent][j - 1] = model->children[parent][j];
    --model->count[parent]; model->parent[child] = -1;
}

static bool append(tree_model* model, unsigned parent, unsigned child, int budget)
{
    bool expected = allowed(model, parent, child);
    xrtClearError();
    if (budget >= 0) require(xrtMemDebugFailAfter((uint64)budget), "arm append OOM");
    bool actual = xrtXmlDomAppendChild(model->nodes[parent], model->nodes[child]);
    bool triggered = budget >= 0 && xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (triggered) { ++oom_count; require(expected && !actual && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM rolls back valid append"); }
    else {
        require(actual == expected, "append legality/model");
        require(xrtGetError() == NULL, "logical refusal does not invent allocation error");
        if (!expected) ++invalid_count;
        if (actual && expected) {
            detach(model, child); model->children[parent][model->count[parent]++] = (int)child; model->parent[child] = (int)parent;
        }
    }
    verify(model);
    return actual;
}

static void remove_node(tree_model* model, unsigned parent, unsigned child)
{
    bool expected = model->parent[child] == (int)parent;
    bool actual = xrtXmlDomRemoveChild(model->nodes[parent], model->nodes[child]);
    require(actual == expected, "remove legality/model");
    if (actual && expected) detach(model, child);
    verify(model);
}

static void type_matrix(void)
{
    static const unsigned parents[] = {0,1,13,17,21}, children[] = {0,2,14,18,22};
    for (unsigned attached = 0; attached < 2; ++attached) for (unsigned p = 0; p < 5; ++p) for (unsigned c = 0; c < 5; ++c) {
        tree_model model;
        xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
        bool ready = create(&model); require(ready, "matrix fixture create");
        if (ready) {
            if (attached) append(&model, 0, p == 1 ? 1 : 12, -1);
            bool actual = append(&model, parents[p], children[c], -1);
            if (print_types) printf("%u\t%u\t0\t%u\t%u\t%u\n", matrix_cases, attached, kind(parents[p]), kind(children[c]), (unsigned)actual);
            ++matrix_cases;
        }
        destroy(&model); require(balanced(&before), "type matrix memory balanced");
    }
    tree_model left, right;
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    bool a = create(&left), b = create(&right); require(a && b, "cross document fixture");
    if (a && b) for (unsigned p = 0; p < 5; ++p) for (unsigned c = 0; c < 5; ++c) {
        bool actual = xrtXmlDomAppendChild(left.nodes[parents[p]], right.nodes[children[c]]);
        require(!actual, "foreign document rejected without adoption");
        if (print_types) printf("%u\t0\t1\t%u\t%u\t%u\n", matrix_cases, kind(parents[p]), kind(children[c]), (unsigned)actual);
        verify(&left); verify(&right); ++matrix_cases;
    }
    destroy(&left); destroy(&right); require(balanced(&before), "foreign document memory balanced");
}

static void growth_failures(void)
{
    static const unsigned widths[] = {0,4,8,16};
    for (unsigned i = 0; i < 4; ++i) {
        tree_model model;
        xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
        bool ready = create(&model); require(ready, "growth fixture");
        if (ready) {
            append(&model, 0, 1, -1); append(&model, 1, 2, -1); append(&model, 1, 3, -1);
            for (unsigned j = 0; j < widths[i]; ++j) append(&model, 2, 4+j, -1);
            append(&model, 3, 24, -1);
            xmemdebugsnapshot stable; xrtClearError(); xrtMemDebugSnapshot(&stable);
            require(!append(&model, 2, 24, 0), "full target append must hit growth OOM");
            require(balanced(&stable), "failed growth preserves both trees and memory");
            require(append(&model, 2, 24, -1), "retry identical append after OOM");
            require(append(&model, 2, 24, 0), "same-parent append reorders without allocation");
        }
        destroy(&model); require(balanced(&before), "growth fixture memory balanced");
    }
}

static uint32 next(uint32* seed) { *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5; return *seed; }

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--types") != 0)) return 2;
    print_types = argc == 2;
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.xml.dom.mutation", 0, "warm error"); xrtClearError();
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    type_matrix(); growth_failures();
    for (unsigned seed = 1; seed <= SEEDS; ++seed) {
        tree_model model; uint32 random = seed * 0x9e3779b9u;
        xmemdebugsnapshot start; xrtClearError(); xrtMemDebugSnapshot(&start);
        bool ready = create(&model); require(ready, "sequence fixture");
        unsigned old_failures = failures;
        if (ready) for (unsigned step = 0; step < STEPS; ++step) {
            unsigned parent = next(&random) % NODES, child = next(&random) % NODES;
            uint32 operation = next(&random);
            if (operation % 5 == 0) remove_node(&model, parent, child);
            else append(&model, parent, child, operation % 3 == 0 ? 0 : -1);
            ++steps_run;
            if (failures != old_failures) break; /* 旧代码/负控失配后不继续驱动已分歧状态。 */
        }
        ++seeds_run; destroy(&model); require(balanced(&start), "sequence memory balanced");
    }
    require(matrix_cases == 75 && seeds_run == SEEDS && steps_run == SEEDS * STEPS, "entire declared matrix and sequences executed");
    require(oom_count >= 4 && invalid_count > 0, "actual OOM and refusal paths covered");
    require(balanced(&before), "all model owners retired");
    if (!print_types) printf("XML DOM mutation: %u matrix, %u seeds, %u steps, %u checks, %u OOM, %u refusals, %u failures\n",
        matrix_cases, seeds_run, steps_run, checks, oom_count, invalid_count, failures);
    return failures ? 1 : 0;
}

#include "../xrt_xml_dom_tree.c"
#include "../xrt_xml_dom_parser.c"
#include "../xrt_xml_dom_writer.c"
#include "../../xml.sax/xrt_xml_common.c"
#include "../../xml.sax/xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
