#include "foreign_xui_table_column_formatter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct formatter_owner_t {
    xui_widget table;
    size_t references;
    bool replaceOnHold;
    int rendered;
} formatter_owner_t;

static bool hold(const void* identity)
{
    formatter_owner_t* owner = (formatter_owner_t*)identity;
    ++owner->references;
    if (owner->replaceOnHold) {
        owner->replaceOnHold = false;
        xui_table_view_column_t newer = {.sTitle="Nested"};
        assert(xuiTableViewSetColumnsCopy(owner->table, &newer, 1) == XUI_OK);
    }
    return true;
}

static bool drop(const void* identity)
{
    formatter_owner_t* owner = (formatter_owner_t*)identity;
    assert(owner->references > 1);
    --owner->references;
    return true;
}

static bool references(const void* identity, size_t* output)
{
    *output = ((const formatter_owner_t*)identity)->references;
    return true;
}

static bool trace(const void* identity, xrtownershipvisitor visit, void* user)
{
    (void)identity;
    (void)visit;
    (void)user;
    return true;
}

static const xrtownershipops operations = {references, trace};

static int formatter(xui_widget table, int row, int column, const xui_table_view_cell_t* cell,
    xui_table_view_format_output_t* output, void* user)
{
    (void)table;
    (void)row;
    (void)column;
    (void)cell;
    assert(((formatter_owner_t*)user)->references > 1);
    return xuiTableViewFormatOutputSetText(output, (xstrview){"measured",8});
}

static int raw_formatter(xui_widget table, int row, int column, const xui_table_view_cell_t* cell,
    char* output, int capacity, void* user)
{
    (void)table;
    (void)row;
    (void)column;
    (void)cell;
    (void)user;
    snprintf(output, (size_t)capacity, "%s", "Raw text");
    return 1;
}

static int renderer(xui_widget table, int row, int column, const xui_table_view_cell_t* cell,
    xui_draw_context draw, xui_rect_t rect, int flags, void* user)
{
    (void)table;
    (void)row;
    (void)column;
    (void)cell;
    (void)draw;
    (void)rect;
    (void)flags;
    formatter_owner_t* owner = user;
    assert(owner->references > 1);
    ++owner->rendered;
    return 1;
}

void fixtureTableCheckGridRows(xui_widget grid, int count, int firstHeight)
{
    xui_widget table = xuiTableGridGetTableView(grid);
    assert(xuiTableViewGetRowCount(table) == count);
    if (count) {
        xui_rect_t rect = {0};
        assert(xuiTableViewGetCellContentRect(table, 0, 0, &rect) == XUI_OK);
        assert(rect.fH == firstHeight);
    }
}

void fixtureTableUseRawFormatter(xui_widget table)
{
    assert(xuiTableViewSetColumnFormatter(table, 0, raw_formatter, NULL) == XUI_OK);
}

void fixtureTableCheckFormatterOwners(xui_widget table)
{
    formatter_owner_t state = {.table=table,.references=1};
    const xui_callback_owner_t owner = {{&state,&operations},hold,drop};
    xui_table_view_column_t columns[XUI_TABLE_VIEW_COLUMN_CAPACITY] = {{0}};
    assert(xuiTableViewSetColumns(table, columns, XUI_TABLE_VIEW_COLUMN_CAPACITY) == XUI_OK);
    for (int column = 0; column < XUI_TABLE_VIEW_COLUMN_CAPACITY; ++column) {
        assert(xuiTableViewSetColumnFormatterOwned(table, column, formatter, &state, &owner) == XUI_OK);
        assert(xuiTableViewSetColumnRendererOwned(table, column, renderer, &state, &owner) == XUI_OK);
    }
    assert(state.references == 2 * XUI_TABLE_VIEW_COLUMN_CAPACITY + 1);

    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(0));
    assert(xuiTableViewSetColumnsCopy(table, columns, 1) == XUI_ERROR_OUT_OF_MEMORY);
    xrtMemDebugFailClear();
    xrtErrorFree(xrtTakeError());
    assert(state.references == 2 * XUI_TABLE_VIEW_COLUMN_CAPACITY + 1);

    assert(xuiTableViewSetColumns(table, columns, XUI_TABLE_VIEW_COLUMN_CAPACITY) == XUI_OK);
    assert(state.references == 1);
    state.replaceOnHold = true;
    assert(xuiTableViewSetColumnRendererOwned(table, 0, renderer, &state, &owner) == XUI_ERROR_INVALID_STATE);
    xrtErrorFree(xrtTakeError());
    assert(state.references == 1);
    xui_table_column_desc_t actual = {0};
    assert(xuiTableViewGetColumnDescriptor(table, 0, &actual) == XUI_OK);
    assert(actual.sTitle && strcmp(actual.sTitle,"Nested") == 0);
}

void fixtureTableMeasureFormatter(xui_widget table, int native)
{
    formatter_owner_t state = {.table=table,.references=1};
    const xui_callback_owner_t owner = {{&state,&operations},hold,drop};
    if (native)
        assert(xuiTableViewSetColumnFormatterOwned(table, 0, formatter, &state, &owner) == XUI_OK);

    const int rounds = 5000;
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xui_accessible_node_t node = {.iSize=sizeof(node)};
    assert(xuiWidgetGetAccessibleNode(table, 1, &node) == XUI_OK);
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    clock_t start = clock();
    for (int i = 0; i < rounds; ++i) {
        assert(xuiWidgetGetAccessibleNode(table, 1, &node) == XUI_OK);
        assert(node.sName && strcmp(node.sName,"measured") == 0);
    }
    clock_t end = clock();
    xrtMemDebugSnapshot(&after);
    printf("Table column formatter %s: rounds=%d clock_ms=%.3f alloc=%llu free=%llu realloc=%llu\n",
        native ? "native" : "language", rounds, (double)(end-start)*1000.0/CLOCKS_PER_SEC,
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));

    if (native) {
        assert(xuiTableViewSetColumnFormatterOwned(table, 0, NULL, NULL, NULL) == XUI_OK);
        assert(state.references == 1);
    }
}

void fixtureTableMeasureColumnRenderer(xui_widget table, xui_surface target, int native)
{
    formatter_owner_t state = {.table=table,.references=1};
    const xui_callback_owner_t owner = {{&state,&operations},hold,drop};
    if (native)
        assert(xuiTableViewSetColumnRendererOwned(table, 0, renderer, &state, &owner) == XUI_OK);
    xui_context ui = xuiWidgetGetContext(table);
    xui_widget viewport = xuiTableViewGetViewportWidget(table);
    assert(viewport && xuiRender(ui, target, NULL, 0) == XUI_OK);
    const int rounds = 500;
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    clock_t start = clock();
    for (int i = 0; i < rounds; ++i) {
        assert(xuiWidgetInvalidate(viewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER) == XUI_OK);
        assert(xuiRender(ui, target, NULL, 0) == XUI_OK);
    }
    clock_t end = clock();
    xrtMemDebugSnapshot(&after);
    printf("Table column renderer %s: rounds=%d clock_ms=%.3f alloc=%llu free=%llu realloc=%llu\n",
        native ? "native" : "language", rounds, (double)(end-start)*1000.0/CLOCKS_PER_SEC,
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
    if (native) {
        assert(state.rendered == rounds + 1);
        assert(xuiTableViewSetColumnRendererOwned(table, 0, NULL, NULL, NULL) == XUI_OK);
        assert(state.references == 1);
    }
}
