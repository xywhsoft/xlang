#include "src/sem/xl6_protocol.h"

#include <string.h>

#define XL6_PROTOCOL_ANY_COUNT (-1)

static const XL6_ProtocolRule g_xl6ProtocolRules[] = {
    {"__create__", XL6_PROTOCOL_CREATE, 0, XL6_PROTOCOL_ANY_COUNT, 0, 0, 1, NULL, NULL},
    {"__destroy__", XL6_PROTOCOL_DESTROY, 0, 0, 0, 0, 0, NULL, NULL},

    {"__typeName__", XL6_PROTOCOL_TYPE_NAME, 0, 0, 1, 1, 0, NULL, "string"},
    {"__toString__", XL6_PROTOCOL_TO_STRING, 0, 0, 1, 1, 0, NULL, "string"},
    {"__hash__", XL6_PROTOCOL_HASH, 0, 0, 1, 1, 0, NULL, "int"},
    {"__equals__", XL6_PROTOCOL_EQUALS, 1, 1, 1, 1, 0, NULL, "bool"},

    {"__copy__", XL6_PROTOCOL_COPY, 0, 0, 1, 1, 0, NULL, NULL},
    {"__clone__", XL6_PROTOCOL_CLONE, 0, 0, 1, 1, 0, NULL, NULL},
    {"__move__", XL6_PROTOCOL_MOVE, 0, 0, 1, 1, 0, NULL, NULL},
    {"__copyable__", XL6_PROTOCOL_COPYABLE, 0, 0, 1, 1, 0, NULL, "bool"},
    {"__toValue__", XL6_PROTOCOL_TO_VALUE, 0, 0, 1, 1, 0, NULL, "var"},
    {"__fromValue__", XL6_PROTOCOL_FROM_VALUE, 1, 1, 1, 1, 0, "var", "bool"},

    {"__item__", XL6_PROTOCOL_ITEM, 1, 1, 1, 1, 0, NULL, NULL},
    {"__setItem__", XL6_PROTOCOL_SET_ITEM, 2, 2, 0, 0, 0, NULL, NULL},
    {"__field__", XL6_PROTOCOL_FIELD, 1, 1, 1, 1, 0, "string", NULL},
    {"__setField__", XL6_PROTOCOL_SET_FIELD, 2, 2, 0, 0, 0, "string", NULL},
    {"__callField__", XL6_PROTOCOL_CALL_FIELD, 1, XL6_PROTOCOL_ANY_COUNT, 0, 1, 1, "string", NULL},

    {"__add__", XL6_PROTOCOL_ADD, 1, 1, 1, 1, 0, NULL, NULL},
    {"__sub__", XL6_PROTOCOL_SUB, 1, 1, 1, 1, 0, NULL, NULL},
    {"__mul__", XL6_PROTOCOL_MUL, 1, 1, 1, 1, 0, NULL, NULL},
    {"__div__", XL6_PROTOCOL_DIV, 1, 1, 1, 1, 0, NULL, NULL},
    {"__mod__", XL6_PROTOCOL_MOD, 1, 1, 1, 1, 0, NULL, NULL},
    {"__pow__", XL6_PROTOCOL_POW, 1, 1, 1, 1, 0, NULL, NULL},

    {"__neg__", XL6_PROTOCOL_NEG, 0, 0, 1, 1, 0, NULL, NULL},
    {"__pos__", XL6_PROTOCOL_POS, 0, 0, 1, 1, 0, NULL, NULL},
    {"__not__", XL6_PROTOCOL_NOT, 0, 0, 1, 1, 0, NULL, "bool"},
    {"__bitNot__", XL6_PROTOCOL_BIT_NOT, 0, 0, 1, 1, 0, NULL, NULL},

    {"__eq__", XL6_PROTOCOL_EQ, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__ne__", XL6_PROTOCOL_NE, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__lt__", XL6_PROTOCOL_LT, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__le__", XL6_PROTOCOL_LE, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__gt__", XL6_PROTOCOL_GT, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__ge__", XL6_PROTOCOL_GE, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__approx__", XL6_PROTOCOL_APPROX, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__and__", XL6_PROTOCOL_AND, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__or__", XL6_PROTOCOL_OR, 1, 1, 1, 1, 0, NULL, "bool"},

    {"__len__", XL6_PROTOCOL_LEN, 0, 0, 1, 1, 0, NULL, "int"},
    {"__contains__", XL6_PROTOCOL_CONTAINS, 1, 1, 1, 1, 0, NULL, "bool"},
    {"__iter__", XL6_PROTOCOL_ITER, 0, 0, 1, 1, 0, NULL, NULL},
    {"__next__", XL6_PROTOCOL_NEXT, 0, 0, 2, 2, 0, NULL, "bool"},
    {"__call__", XL6_PROTOCOL_CALL, 0, XL6_PROTOCOL_ANY_COUNT, 0, XL6_PROTOCOL_ANY_COUNT, 1, NULL, NULL}
};

const XL6_ProtocolRule* xl6ProtocolFindRule(const char* name)
{
    size_t i;

    if ( name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < sizeof(g_xl6ProtocolRules) / sizeof(g_xl6ProtocolRules[0]); ++i ) {
        if ( strcmp(g_xl6ProtocolRules[i].name, name) == 0 ) {
            return &g_xl6ProtocolRules[i];
        }
    }
    return NULL;
}

int xl6ProtocolLooksReservedName(const char* name)
{
    size_t len;

    if ( name == NULL ) {
        return 0;
    }
    len = strlen(name);
    return len >= 5 &&
           name[0] == '_' &&
           name[1] == '_' &&
           name[len - 2] == '_' &&
           name[len - 1] == '_';
}

const char* xl6ProtocolKindName(XL6_ProtocolKind kind)
{
    size_t i;

    for ( i = 0; i < sizeof(g_xl6ProtocolRules) / sizeof(g_xl6ProtocolRules[0]); ++i ) {
        if ( g_xl6ProtocolRules[i].kind == kind ) {
            return g_xl6ProtocolRules[i].name;
        }
    }
    return "<none>";
}
