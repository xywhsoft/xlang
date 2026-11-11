#ifndef FOREIGN_PACK_CALLBACK_NESTED_H
#define FOREIGN_PACK_CALLBACK_NESTED_H
typedef struct PcNestedDesc {int value;int (*callback)(int,void*);void* user;} PcNestedDesc;
static inline int pcNestedCall(const PcNestedDesc* d)
{return d->callback ? d->callback(d->value,d->user):d->value;}
#endif
