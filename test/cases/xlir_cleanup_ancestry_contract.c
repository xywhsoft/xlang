/* Compile the real private helpers and compare them with the independent
 * child-edge reference on arbitrary node-id orders, not just parser preorder.
 * Link the source cohort without its xl6_ir.o and source-driver main. */
#include "src/ir/xl6_ir.c"

#include <stdio.h>
#include <stdlib.h>

static unsigned long long containsChecks, lifetimeChecks;
static unsigned int randomState = 0x78e21345u;

static unsigned int nextRandom(void)
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

static int referenceContains(const XL6_IrModule* module,
    XL6_IrNodeId ancestor, XL6_IrNodeId descendant)
{
    const XL6_IrNode* node;
    size_t i;
    if (ancestor == descendant) return 1;
    node = xl6IrNodeGet(module, ancestor);
    if (node == NULL) return 0;
    for (i = 0; i < node->childCount; ++i)
        if (referenceContains(module, module->children[node->firstChild + i], descendant)) return 1;
    return 0;
}

static int referenceLive(const XL6_IrModule* module,
    XL6_IrNodeId declaration, XL6_IrNodeId exitOrigin)
{
    const XL6_IrNode* candidate = xl6IrNodeGet(module, declaration);
    size_t i;
    if (candidate != NULL && candidate->kind == XL6_IR_NODE_PATTERN) {
        XL6_IrNodeId scope = xl6IrPatternBindingScope(module, declaration);
        return scope != 0 && scope != exitOrigin && referenceContains(module, scope, exitOrigin);
    }
    if (candidate == NULL || candidate->region == 0 ||
        referenceContains(module, declaration, exitOrigin)) return 0;
    for (i = 0; i < module->nodeCount; ++i) {
        const XL6_IrNode* scope = &module->nodes[i];
        XL6_IrNodeId id = (XL6_IrNodeId)(i + 1);
        if (scope->kind == XL6_IR_NODE_BLOCK && scope->region == candidate->region &&
            referenceContains(module, id, declaration)) return referenceContains(module, id, exitOrigin);
    }
    return 0;
}

static void require(int condition, const char* name)
{
    if (!condition) {
        fprintf(stderr, "cleanup ancestry: %s\n", name);
        exit(1);
    }
}

static void compareTree(const XL6_IrModule* module)
{
    XL6_IrNodeId a, b;
    XL6_DiagList diags;
    xl6DiagInit(&diags);
    require(xl6IrVerifyNodeTopology(module, &diags), "reference topology is valid");
    xl6DiagFree(&diags);
    for (a = 0; a <= module->nodeCount + 1; ++a) {
        for (b = 0; b <= module->nodeCount + 1; ++b) {
            if (referenceContains(module, a, b) != xl6IrNodeContains(module, a, b)) {
                fprintf(stderr, "containment differs: %u %u\n", (unsigned)a, (unsigned)b);
                exit(1);
            }
            ++containsChecks;
            if (referenceLive(module, a, b) != xl6IrCleanupDeclarationIsLiveAtExit(module, a, b)) {
                fprintf(stderr, "lifetime differs: %u %u\n", (unsigned)a, (unsigned)b);
                exit(1);
            }
            ++lifetimeChecks;
        }
    }
}

static void buildChildren(XL6_IrModule* module)
{
    size_t i, j, count = 0;
    for (i = 0; i < module->nodeCount; ++i) {
        module->nodes[i].firstChild = count;
        for (j = 0; j < module->nodeCount; ++j) {
            if (module->nodes[j].parent == i + 1)
                module->children[count++] = (XL6_IrNodeId)(j + 1);
        }
        module->nodes[i].childCount = count - module->nodes[i].firstChild;
    }
    module->childCount = count;
    require(count + 1 == module->nodeCount, "tree edge population");
}

static void permutedTrees(void)
{
    XL6_IrModule module = {0};
    XL6_IrNode nodes[64];
    XL6_IrNodeId children[64], order[64];
    size_t tree, i;
    module.nodes = nodes;
    module.nodeCount = 64;
    module.children = children;
    for (tree = 0; tree < 256; ++tree) {
        memset(nodes, 0, sizeof(nodes));
        for (i = 0; i < 64; ++i) order[i] = (XL6_IrNodeId)(i + 1);
        for (i = 63; i != 0; --i) {
            size_t j = nextRandom() % (i + 1);
            XL6_IrNodeId temporary = order[i];
            order[i] = order[j]; order[j] = temporary;
        }
        module.root = order[0];
        for (i = 0; i < 64; ++i) {
            XL6_IrNode* node = &nodes[order[i] - 1];
            /* Cover deep, wide and mixed trees, several equal-region block
             * ancestors, and declarations with no matching region block. */
            node->parent = i == 0 ? 0 : order[tree % 3 == 0 ? i - 1 :
                tree % 3 == 1 ? 0 : nextRandom() % i];
            node->region = nextRandom() % 5;
            node->kind = i % 4 == 0 ? XL6_IR_NODE_BLOCK : i % 4 == 1 ?
                XL6_IR_NODE_VALUE_DECL : i % 4 == 2 ? XL6_IR_NODE_NAME : XL6_IR_NODE_PATTERN;
        }
        buildChildren(&module);
        compareTree(&module);
    }
}

static void patternCase(void)
{
    XL6_IrModule module = {0};
    XL6_IrNode nodes[8] = {{0}};
    XL6_IrNodeId children[8];
    size_t i;
    module.nodes = nodes; module.nodeCount = 8;
    module.children = children; module.root = 8;
    for (i = 0; i < 8; ++i) { nodes[i].kind = XL6_IR_NODE_NAME; nodes[i].region = 1; }
    nodes[7].kind = XL6_IR_NODE_BLOCK;
    nodes[5].kind = XL6_IR_NODE_MATCH_CASE; nodes[5].parent = 8;
    nodes[0].kind = XL6_IR_NODE_PATTERN; nodes[0].sourceKind = XL6_AST_PATTERN_VARIANT; nodes[0].parent = 6;
    nodes[3].kind = XL6_IR_NODE_BLOCK; nodes[3].parent = 6;
    nodes[1].parent = 1; nodes[2].parent = 4;
    nodes[4].kind = XL6_IR_NODE_VALUE_DECL; nodes[4].parent = 4;
    nodes[6].parent = 8;
    buildChildren(&module);
    require(xl6IrCleanupDeclarationIsLiveAtExit(&module, 1, 3), "live case binding");
    require(!xl6IrCleanupDeclarationIsLiveAtExit(&module, 1, 4), "already closed case binding");
    require(!xl6IrCleanupDeclarationIsLiveAtExit(&module, 1, 2), "pattern initializer");
    require(!xl6IrCleanupDeclarationIsLiveAtExit(&module, 1, 7), "sibling outside case");
    compareTree(&module);
}

static void malformedAncestry(void)
{
    XL6_IrModule module = {0};
    XL6_IrNode nodes[3] = {{0}};
    XL6_IrNodeId children[2] = {2, 1};
    XL6_DiagList diags;
    module.nodes = nodes; module.nodeCount = 3; module.root = 3;
    module.children = children; module.childCount = 2;
    nodes[0].kind = XL6_IR_NODE_VALUE_DECL; nodes[0].region = 1;
    nodes[0].parent = 2; nodes[0].childCount = 1;
    nodes[1].parent = 1; nodes[1].firstChild = 1; nodes[1].childCount = 1;
    xl6DiagInit(&diags);
    require(!xl6IrVerifyNodeTopology(&module, &diags), "disconnected reciprocal cycle rejected");
    xl6DiagFree(&diags);
    require(!xl6IrNodeContains(&module, 3, 1), "bounded parent cycle");
    require(!xl6IrCleanupDeclarationIsLiveAtExit(&module, 1, 3), "cyclic scope refused");
    nodes[1].parent = 4;
    xl6DiagInit(&diags);
    require(!xl6IrVerifyNodeTopology(&module, &diags), "invalid parent rejected");
    xl6DiagFree(&diags);
    require(!xl6IrNodeContains(&module, 3, 1), "out of range parent refused");
    require(!xl6IrCleanupDeclarationIsLiveAtExit(&module, 1, 3), "out of range scope refused");
}

int main(void)
{
    permutedTrees();
    patternCase();
    malformedAncestry();
    printf("cleanup ancestry passed: trees=257 contains=%llu lifetimes=%llu malformed=6\n",
        containsChecks, lifetimeChecks);
    return 0;
}
