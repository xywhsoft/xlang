#ifndef XLANG_ANALYSIS_FRONTEND_SEM_READ_VALUES_H
#define XLANG_ANALYSIS_FRONTEND_SEM_READ_VALUES_H
#include <xrt.h>
#include <string.h>
#include "src/sem/xl6_type_registry.h"

/* A complete owned value for a registry result with no declaration tree.
 * The original resolver still runs on every query. This only compares its
 * actual result without repeatedly encoding the same scalar/string fields.
 * Type-registry source/header hashes in the producer guard this field audit. */
typedef struct xls_sem_read_value {
    XL6_TypeInfo type;
    size_t stringsBytes;
    int ready,present;
} xls_sem_read_value;
static inline void xlsSemReadValueUnit(xls_sem_read_value* value)
{
    if(!value)return;
    xrtFree(value->type.name);xrtFree(value->type.abiName);*value=(xls_sem_read_value){0};
}
static inline int xlsSemReadValueInit(xls_sem_read_value* value,const XL6_TypeInfo* info)
{
    /* The caller supplies an empty value;failure leaves it empty so the
     * mandatory serialized snapshot can remain the comparison fallback. */
    if(!value || (info && info->decl))return 0;
    xls_sem_read_value owned={.ready=1,.present=info!=NULL};
    if(info){
        owned.type=*info;owned.type.name=NULL;owned.type.abiName=NULL;
        if(info->name){owned.type.name=xrtStrDup(info->name);owned.stringsBytes+=strlen(info->name)+1;}
        if(info->abiName){owned.type.abiName=xrtStrDup(info->abiName);owned.stringsBytes+=strlen(info->abiName)+1;}
        if((info->name && !owned.type.name) || (info->abiName && !owned.type.abiName)){xlsSemReadValueUnit(&owned);return 0;}
    }
    *value=owned;return 1;
}
static inline int xlsSemReadValueText(const char* a,const char* b)
{return a && b ? !strcmp(a,b) : a==b;}
static inline int xlsSemReadValueEqual(const xls_sem_read_value* value,const XL6_TypeInfo* info)
{
    if(!value || !value->ready)return 0;
    if(!value->present)return info==NULL;
    if(!info || info->decl)return 0;
    const XL6_TypeInfo* old=&value->type;
    return old->id==info->id && old->abiId==info->abiId && old->kind==info->kind &&
        old->allowGeneric==info->allowGeneric && old->genericArity==info->genericArity &&
        xlsSemReadValueText(old->name,info->name) && xlsSemReadValueText(old->abiName,info->abiName);
}
#endif
