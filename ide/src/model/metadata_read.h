#ifndef XLANG_IDE_METADATA_READ_H
#define XLANG_IDE_METADATA_READ_H
#include "app_xrt.h"

/* Optional persisted fields and schema tests are probes, not failed checked
 * casts. Absence/type mismatch leaves the thread's diagnostic unchanged.
 * Access/ownership failures on an otherwise valid type still propagate. */
static inline bool xideMetaString(const xvalue* v,xstrview* out)
{ return v && xrtValueIs(v,XVALUE_STRING) && xrtValueGetString(v,out); }
static inline bool xideMetaBool(const xvalue* v,bool* out)
{ return v && xrtValueIs(v,XVALUE_BOOL) && xrtValueGetBool(v,out); }
static inline bool xideMetaInt(const xvalue* v,int64* out)
{ return v && xrtValueIs(v,XVALUE_INT) && xrtValueGetInt(v,out); }
static inline bool xideMetaUInt(const xvalue* v,uint64* out)
{ return v && xrtValueIs(v,XVALUE_UINT) && xrtValueGetUInt(v,out); }
static inline bool xideMetaFloat(const xvalue* v,double* out)
{ return v && xrtValueIs(v,XVALUE_FLOAT) && xrtValueGetFloat(v,out); }
static inline xvalue* xideMetaField(const xvalue* v,xstrview key)
{ return v && xrtValueIs(v,XVALUE_OBJECT) ? xrtValueObjectGet(v,key):NULL; }
static inline size_t xideMetaCount(const xvalue* v)
{ return v && xrtValueIsContainer(v) ? xrtValueCount(v):0; }
#endif
