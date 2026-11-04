#ifndef FOREIGN_VOID_FACTORY_H
#define FOREIGN_VOID_FACTORY_H
#include "test/cases/foreign_inplace_factory.h"
void inplace_void_plain_init(PlainRecord*,int);
void inplace_void_init(InplaceRecord*,int);
void inplace_void_plain_from(PlainRecord*,PlainRecord*);
#endif
