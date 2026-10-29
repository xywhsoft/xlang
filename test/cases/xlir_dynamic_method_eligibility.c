/* Private compiler unit: include the real implementation, not a test copy of
 * the optimized algorithm. Dependencies are linked as ordinary source objects.
 * The independent full-scan oracle retains the pre-optimization decision rule. */
#include "src/ir/xl6_ir.c"
#include <assert.h>

static int reference_eligible(const XL6_IrModule* module,
    const XL6_IrClassLayout* layout, XL6_IrNodeId declaration,
    const XL6_IrNode* method)
{
    const XL6_IrClassLayout* current = layout;
    size_t depth = 0;
    if (!module || !layout || !method || method->kind != XL6_IR_NODE_FUNC_DECL ||
        (method->sourceKind != XL6_AST_FUNC && method->sourceKind != XL6_AST_GET &&
         method->sourceKind != XL6_AST_LET && method->sourceKind != XL6_AST_ASYNC_FUNC &&
         method->sourceKind != XL6_AST_EXPORT_FUNC && method->sourceKind != XL6_AST_NATIVE_EXTERN_FUNC) ||
        (method->flags & (XL6_AST_FLAG_C_OWNED_SPECIALIZATION | XL6_AST_FLAG_STATIC |
                          XL6_AST_FLAG_PRIVATE | XL6_AST_FLAG_PROTECTED)) || !method->symbol) return 0;
    const char* name = xl6IrBuildClassMethodName(module, declaration);
    if (!name) return 0;
    XL6_IrNodeId owner = xl6IrBuildEnclosingClass(module, declaration);
    while (current && depth++ <= module->classLayoutCount) {
        if (current->declaration == owner) return 1;
        for (size_t i = 0; i < module->nodeCount; ++i) {
            const XL6_IrNode* candidate = &module->nodes[i];
            if (xl6IrDynamicMethodDeclarationKind(candidate) != xl6IrDynamicMethodDeclarationKind(method) ||
                xl6IrBuildEnclosingClass(module, (XL6_IrNodeId)(i + 1)) != current->declaration) continue;
            const char* spelling = xl6IrBuildClassMethodName(module, (XL6_IrNodeId)(i + 1));
            if (spelling && !strcmp(spelling, name) &&
                xl6IrDynamicMethodSameOverload(module, declaration, (XL6_IrNodeId)(i + 1))) return 0;
        }
        current = xl6IrBuildClassLayoutByType(module, current->baseType);
    }
    return 0;
}

typedef struct Fixture {
    XL6_IrModule module;
    XL6_IrNode nodes[128];
    XL6_IrNodeId children[256];
    XL6_IrType types[10];
    XL6_IrClassLayout layouts[7];
} Fixture;

static XL6_IrNodeId add_node(Fixture* fixture, XL6_IrNodeKind kind)
{
    assert(fixture->module.nodeCount < sizeof(fixture->nodes) / sizeof(*fixture->nodes));
    XL6_IrNodeId id = (XL6_IrNodeId)++fixture->module.nodeCount;
    fixture->nodes[id - 1].kind = kind;
    return id;
}

static XL6_IrNodeId add_method(Fixture* fixture, size_t owner,
    const char* name, XL6_AstKind kind, XL6_IrTypeId argument)
{
    XL6_IrNodeId method = add_node(fixture, XL6_IR_NODE_FUNC_DECL);
    XL6_IrNodeId spelling = add_node(fixture, XL6_IR_NODE_NAME);
    XL6_IrNodeId parameters = add_node(fixture, XL6_IR_NODE_LIST);
    XL6_IrNode* node = &fixture->nodes[method - 1];
    node->sourceKind = kind;
    node->symbol = method;
    node->parent = fixture->layouts[owner].declaration;
    node->firstChild = fixture->module.childCount;
    node->childCount = 2;
    fixture->children[fixture->module.childCount++] = spelling;
    fixture->children[fixture->module.childCount++] = parameters;
    fixture->nodes[spelling - 1].text = (char*)name;
    fixture->nodes[spelling - 1].parent = method;
    fixture->nodes[parameters - 1].parent = method;
    if (argument) {
        XL6_IrNodeId parameter = add_node(fixture, XL6_IR_NODE_PARAM_DECL);
        fixture->nodes[parameters - 1].firstChild = fixture->module.childCount;
        fixture->nodes[parameters - 1].childCount = 1;
        fixture->children[fixture->module.childCount++] = parameter;
        fixture->nodes[parameter - 1].parent = parameters;
        fixture->nodes[parameter - 1].storageType = argument;
    }
    return method;
}

static void initialize(Fixture* fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    fixture->module.nodes = fixture->nodes;
    fixture->module.children = fixture->children;
    fixture->module.types.items = fixture->types;
    fixture->module.types.count = 10;
    fixture->module.classLayouts = fixture->layouts;
    fixture->module.classLayoutCount = 7;
    for (size_t i = 0; i < 10; ++i) {
        fixture->types[i].kind = i < 3 ? XL6_IR_TYPE_BUILTIN : XL6_IR_TYPE_CLASS;
        fixture->types[i].cAbi = i == 0 ? XL6_IR_C_ABI_I64 : i == 1 ? XL6_IR_C_ABI_F64 :
            i == 2 ? XL6_IR_C_ABI_MANAGED_STRING : XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
        fixture->types[i].ownershipAbi = i < 2 ? XL6_IR_OWNERSHIP_ABI_TRIVIAL : XL6_IR_OWNERSHIP_ABI_MANAGED;
    }
    for (size_t i = 0; i < 7; ++i) {
        XL6_IrNodeId id = add_node(fixture, XL6_IR_NODE_TYPE_DECL);
        fixture->nodes[id - 1].sourceKind = XL6_AST_CLASS;
        fixture->nodes[id - 1].text = "SameSpelling";
        fixture->layouts[i].declaration = id;
        fixture->layouts[i].type = (XL6_IrTypeId)(4 + i);
    }
    fixture->layouts[1].baseType = fixture->layouts[0].type;
    fixture->layouts[2].baseType = fixture->layouts[1].type;
    /* Two concrete layouts may share source declarations. Declaration IDs,
     * not type spelling or the first concrete specialization, decide ancestry. */
    fixture->layouts[5].declaration = fixture->layouts[0].declaration;
    fixture->layouts[6].declaration = fixture->layouts[1].declaration;
    fixture->layouts[6].baseType = fixture->layouts[5].type;
}

static int eligible(Fixture* fixture, size_t owner, XL6_IrNodeId method)
{
    return xl6IrBuildDynamicMethodEligible(&fixture->module, &fixture->layouts[owner],
        method, &fixture->nodes[method - 1]);
}

static size_t compare_all(Fixture* fixture)
{
    size_t checked = 0;
    for (size_t c = 0; c < fixture->module.classLayoutCount; ++c)
        for (size_t n = 0; n < fixture->module.nodeCount; ++n) {
            const XL6_IrNode* node = &fixture->nodes[n];
            assert(xl6IrBuildDynamicMethodEligible(&fixture->module, &fixture->layouts[c],
                (XL6_IrNodeId)(n + 1), node) == reference_eligible(&fixture->module,
                &fixture->layouts[c], (XL6_IrNodeId)(n + 1), node));
            ++checked;
        }
    return checked;
}

int main(void)
{
    Fixture fixture;
    initialize(&fixture);
    XL6_IrNodeId baseInt = add_method(&fixture, 0, "pick", XL6_AST_FUNC, 1);
    XL6_IrNodeId baseFloat = add_method(&fixture, 0, "pick", XL6_AST_FUNC, 2);
    XL6_IrNodeId childInt = add_method(&fixture, 1, "pick", XL6_AST_FUNC, 1);
    XL6_IrNodeId grandString = add_method(&fixture, 2, "pick", XL6_AST_FUNC, 3);
    XL6_IrNodeId getter = add_method(&fixture, 0, "score", XL6_AST_GET, 0);
    XL6_IrNodeId setter = add_method(&fixture, 0, "score", XL6_AST_LET, 1);
    add_method(&fixture, 1, "score", XL6_AST_GET, 0);
    add_method(&fixture, 3, "pick", XL6_AST_FUNC, 1);
    assert(eligible(&fixture, 0, baseInt));
    assert(!eligible(&fixture, 1, baseInt));
    assert(eligible(&fixture, 1, baseFloat));
    assert(eligible(&fixture, 2, childInt) && eligible(&fixture, 2, baseFloat));
    assert(eligible(&fixture, 2, grandString));
    assert(!eligible(&fixture, 3, baseInt) && !eligible(&fixture, 4, baseFloat));
    assert(!eligible(&fixture, 1, getter) && eligible(&fixture, 1, setter));
    assert(eligible(&fixture, 5, baseInt) && !eligible(&fixture, 6, baseInt));
    assert(eligible(&fixture, 6, baseFloat));
    size_t checked = compare_all(&fixture);
    const XL6_AstKind kinds[] = {XL6_AST_FUNC, XL6_AST_ASYNC_FUNC, XL6_AST_EXPORT_FUNC,
        XL6_AST_NATIVE_EXTERN_FUNC, XL6_AST_GET, XL6_AST_LET, XL6_AST_CREATE, XL6_AST_ANON_FUNC};
    const unsigned flags[] = {0, XL6_AST_FLAG_PRIVATE, XL6_AST_FLAG_PROTECTED,
        XL6_AST_FLAG_STATIC, XL6_AST_FLAG_C_OWNED_SPECIALIZATION};
    XL6_IrNode saved = fixture.nodes[childInt - 1];
    for (size_t k = 0; k < sizeof(kinds) / sizeof(*kinds); ++k)
        for (size_t f = 0; f < sizeof(flags) / sizeof(*flags); ++f) {
            fixture.nodes[childInt - 1] = saved;
            fixture.nodes[childInt - 1].sourceKind = kinds[k];
            fixture.nodes[childInt - 1].flags = flags[f];
            checked += compare_all(&fixture);
        }
    fixture.nodes[childInt - 1] = saved;
    fixture.nodes[childInt - 1].symbol = 0;
    checked += compare_all(&fixture);
    fixture.nodes[childInt - 1] = saved;
    /* Missing base and bounded cycles must not invent an ancestor. */
    fixture.layouts[4].baseType = 999;
    checked += compare_all(&fixture);
    fixture.layouts[4].baseType = fixture.layouts[3].type;
    fixture.layouts[3].baseType = fixture.layouts[4].type;
    assert(!eligible(&fixture, 4, baseInt));
    checked += compare_all(&fixture);
    printf("dynamic method eligibility: %zu decisions match full-scan oracle\n", checked);
    return 0;
}
