#include "src/sem/xl6_sem_field_attrs.h"

#include <string.h>

static int xl6TextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static int xl6SemAstHasFlag(const XL6_AstNode* node, unsigned int flag)
{
    return node != NULL && (node->flags & flag) != 0;
}

static void xl6SemFieldAttrError(
    XL6_DiagList* diags,
    const XL6_AstNode* node,
    const char* message)
{
    if ( diags != NULL ) {
        xl6DiagAdd(
            diags,
            node != NULL ? node->line : 1,
            node != NULL ? node->column : 1,
            message);
    }
}

XL6_AstNode* xl6SemFieldDefaultExpr(const XL6_AstNode* field)
{
    size_t i;

    if ( field == NULL || field->kind != XL6_AST_FIELD ) {
        return NULL;
    }

    /*
        字段 child[0] 固定为类型。
        从 child[1] 开始，可能是默认值，也可能是字段注解。
    */
    for ( i = 1; i < field->childCount; ++i ) {
        XL6_AstNode* child = field->children[i];
        if ( child != NULL && child->kind != XL6_AST_FIELD_ATTR ) {
            return child;
        }
    }
    return NULL;
}

static int xl6SemFieldHasAttrs(const XL6_AstNode* field)
{
    size_t i;

    if ( field == NULL || field->kind != XL6_AST_FIELD ) {
        return 0;
    }
    for ( i = 1; i < field->childCount; ++i ) {
        const XL6_AstNode* child = field->children[i];
        if ( child != NULL && child->kind == XL6_AST_FIELD_ATTR ) {
            return 1;
        }
    }
    return 0;
}

typedef struct XL6_SemFieldPersistConfig {
    int skip;
    int include;
    int required;
} XL6_SemFieldPersistConfig;

static int xl6SemFieldAttrNameIsValid(const char* name)
{
    return xl6TextEquals(name, "json") ||
           xl6TextEquals(name, "xlon") ||
           xl6TextEquals(name, "persist") ||
           xl6TextEquals(name, "string") ||
           xl6TextEquals(name, "compare");
}

static int xl6SemFieldAttrAppliesToFormat(const XL6_AstNode* attr, const char* format)
{
    if ( attr == NULL || attr->kind != XL6_AST_FIELD_ATTR || format == NULL ) {
        return 0;
    }
    return xl6TextEquals(attr->text, "persist") || xl6TextEquals(attr->text, format);
}

static void xl6SemApplyFieldAttrConfig(XL6_SemFieldPersistConfig* cfg, const XL6_AstNode* attr)
{
    size_t i;

    if ( cfg == NULL || attr == NULL ) {
        return;
    }
    for ( i = 0; i < attr->childCount; ++i ) {
        const XL6_AstNode* arg = attr->children[i];
        const XL6_AstNode* key;
        const XL6_AstNode* value;

        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAME ) {
            if ( xl6TextEquals(arg->text, "skip") ) {
                cfg->skip = 1;
            } else if ( xl6TextEquals(arg->text, "include") ) {
                cfg->include = 1;
            } else if ( xl6TextEquals(arg->text, "required") ) {
                cfg->required = 1;
            }
            continue;
        }
        if ( arg->kind != XL6_AST_KEY_VALUE || arg->childCount < 2 ) {
            continue;
        }
        key = arg->children[0];
        value = arg->children[1];
        if ( key == NULL || value == NULL ) {
            continue;
        }
        if ( xl6TextEquals(key->text, "omit") ) {
            /* omit 只影响序列化输出，不参与 skip/include/required 冲突判断。 */
            continue;
        }
    }
}

static void xl6SemCollectFieldAttrConfig(
    const XL6_AstNode* field,
    const char* format,
    XL6_SemFieldPersistConfig* cfg)
{
    size_t i;

    memset(cfg, 0, sizeof(*cfg));
    for ( i = 1; field != NULL && i < field->childCount; ++i ) {
        const XL6_AstNode* attr = field->children[i];
        if ( attr != NULL && xl6TextEquals(attr->text, "persist") ) {
            xl6SemApplyFieldAttrConfig(cfg, attr);
        }
    }
    for ( i = 1; field != NULL && i < field->childCount; ++i ) {
        const XL6_AstNode* attr = field->children[i];
        if ( attr != NULL && attr->kind == XL6_AST_FIELD_ATTR &&
             !xl6TextEquals(attr->text, "persist") &&
             xl6SemFieldAttrAppliesToFormat(attr, format) ) {
            xl6SemApplyFieldAttrConfig(cfg, attr);
        }
    }
}

static int xl6SemCheckFieldAttrArgs(XL6_DiagList* diags, const XL6_AstNode* attr)
{
    int compareLike;
    int seenName;
    int seenSkip;
    int seenInclude;
    int seenRequired;
    int seenOmit;
    int ok;
    size_t i;

    seenName = 0;
    seenSkip = 0;
    seenInclude = 0;
    seenRequired = 0;
    seenOmit = 0;
    ok = 1;

    if ( attr == NULL || attr->kind != XL6_AST_FIELD_ATTR ) {
        return 1;
    }
    compareLike = xl6TextEquals(attr->text, "string") || xl6TextEquals(attr->text, "compare");
    for ( i = 0; i < attr->childCount; ++i ) {
        const XL6_AstNode* arg = attr->children[i];
        const XL6_AstNode* key;
        const XL6_AstNode* value;

        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_STRING ) {
            if ( compareLike ) {
                xl6SemFieldAttrError(diags, arg, "field derive annotation only supports skip");
                ok = 0;
                continue;
            }
            if ( seenName ) {
                xl6SemFieldAttrError(diags, arg, "field persistence annotation has duplicate name option");
                ok = 0;
            }
            seenName = 1;
            continue;
        }
        if ( arg->kind == XL6_AST_NAME ) {
            if ( xl6TextEquals(arg->text, "skip") ) {
                seenSkip += 1;
            } else if ( xl6TextEquals(arg->text, "include") ) {
                if ( compareLike ) {
                    xl6SemFieldAttrError(diags, arg, "field derive annotation only supports skip");
                    ok = 0;
                    continue;
                }
                seenInclude += 1;
            } else if ( xl6TextEquals(arg->text, "required") ) {
                if ( compareLike ) {
                    xl6SemFieldAttrError(diags, arg, "field derive annotation only supports skip");
                    ok = 0;
                    continue;
                }
                seenRequired += 1;
            } else {
                xl6SemFieldAttrError(diags, arg, "unknown field persistence annotation option");
                ok = 0;
            }
            continue;
        }
        if ( arg->kind != XL6_AST_KEY_VALUE || arg->childCount < 2 ) {
            xl6SemFieldAttrError(diags, arg, compareLike ? "field derive annotation only supports skip" : "invalid field persistence annotation option");
            ok = 0;
            continue;
        }

        key = arg->children[0];
        value = arg->children[1];
        if ( key == NULL || value == NULL ) {
            xl6SemFieldAttrError(diags, arg, compareLike ? "field derive annotation only supports skip" : "invalid field persistence annotation option");
            ok = 0;
            continue;
        }
        if ( compareLike ) {
            xl6SemFieldAttrError(diags, key, "field derive annotation only supports skip");
            ok = 0;
            continue;
        }
        if ( xl6TextEquals(key->text, "name") ) {
            if ( value->kind != XL6_AST_STRING ) {
                xl6SemFieldAttrError(diags, value, "field persistence name option must be a string");
                ok = 0;
            }
            if ( seenName ) {
                xl6SemFieldAttrError(diags, key, "field persistence annotation has duplicate name option");
                ok = 0;
            }
            seenName = 1;
        } else if ( xl6TextEquals(key->text, "omit") ) {
            if ( value->kind != XL6_AST_NAME ||
                 (!xl6TextEquals(value->text, "empty") && !xl6TextEquals(value->text, "null")) ) {
                xl6SemFieldAttrError(diags, value, "field persistence omit option must be empty or null");
                ok = 0;
            }
            seenOmit += 1;
        } else {
            xl6SemFieldAttrError(diags, key, "unknown field persistence annotation option");
            ok = 0;
        }
    }

    if ( seenSkip > 1 || seenInclude > 1 || seenRequired > 1 || seenOmit > 1 ) {
        xl6SemFieldAttrError(diags, attr, "field persistence annotation has duplicate option");
        ok = 0;
    }
    if ( seenSkip && seenInclude ) {
        xl6SemFieldAttrError(diags, attr, "field persistence annotation skip conflicts with include");
        ok = 0;
    }
    if ( seenSkip && seenRequired ) {
        xl6SemFieldAttrError(diags, attr, "field persistence annotation skip conflicts with required");
        ok = 0;
    }
    return ok;
}

int xl6SemCheckFieldAttrs(XL6_DiagList* diags, const XL6_AstNode* field)
{
    int seenJson;
    int seenXlon;
    int seenPersist;
    int seenString;
    int seenCompare;
    int ok;
    size_t i;

    if ( field == NULL || field->kind != XL6_AST_FIELD ) {
        return 1;
    }

    if ( xl6SemAstHasFlag(field, XL6_AST_FLAG_STATIC) && xl6SemFieldHasAttrs(field) ) {
        xl6SemFieldAttrError(diags, field, "static field cannot use persistence annotation");
        return 0;
    }

    seenJson = 0;
    seenXlon = 0;
    seenPersist = 0;
    seenString = 0;
    seenCompare = 0;
    ok = 1;
    for ( i = 1; i < field->childCount; ++i ) {
        const XL6_AstNode* attr = field->children[i];
        if ( attr == NULL || attr->kind != XL6_AST_FIELD_ATTR ) {
            continue;
        }
        if ( !xl6SemFieldAttrNameIsValid(attr->text) ) {
            xl6SemFieldAttrError(diags, attr, "unknown field persistence annotation");
            ok = 0;
            continue;
        }
        if ( xl6TextEquals(attr->text, "json") ) {
            seenJson += 1;
        } else if ( xl6TextEquals(attr->text, "xlon") ) {
            seenXlon += 1;
        } else if ( xl6TextEquals(attr->text, "persist") ) {
            seenPersist += 1;
        } else if ( xl6TextEquals(attr->text, "string") ) {
            seenString += 1;
        } else if ( xl6TextEquals(attr->text, "compare") ) {
            seenCompare += 1;
        }
        if ( !xl6SemCheckFieldAttrArgs(diags, attr) ) {
            ok = 0;
        }
    }

    if ( seenJson > 1 || seenXlon > 1 || seenPersist > 1 || seenString > 1 || seenCompare > 1 ) {
        xl6SemFieldAttrError(diags, field, "duplicate field persistence annotation");
        ok = 0;
    }
    if ( ok ) {
        XL6_SemFieldPersistConfig cfg;
        xl6SemCollectFieldAttrConfig(field, "json", &cfg);
        if ( cfg.skip && cfg.include ) {
            xl6SemFieldAttrError(diags, field, "json persistence annotation skip conflicts with include");
            ok = 0;
        }
        if ( cfg.skip && cfg.required ) {
            xl6SemFieldAttrError(diags, field, "json persistence annotation skip conflicts with required");
            ok = 0;
        }
        xl6SemCollectFieldAttrConfig(field, "xlon", &cfg);
        if ( cfg.skip && cfg.include ) {
            xl6SemFieldAttrError(diags, field, "xlon persistence annotation skip conflicts with include");
            ok = 0;
        }
        if ( cfg.skip && cfg.required ) {
            xl6SemFieldAttrError(diags, field, "xlon persistence annotation skip conflicts with required");
            ok = 0;
        }
    }
    return ok;
}

