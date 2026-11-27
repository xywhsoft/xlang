#ifndef XL6_BINDING_MEASURE_VIEWS_H
#define XL6_BINDING_MEASURE_VIEWS_H
typedef struct BindingMeasureVec {float fX,fY;} BindingMeasureVec;
typedef int (*BindingMeasureProc)(BindingMeasureVec*,float*,void*);
int binding_measure_views(BindingMeasureProc,void*);
#endif
