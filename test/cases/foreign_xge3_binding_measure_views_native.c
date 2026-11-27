#include "foreign_xge3_binding_measure_views.h"
#include <assert.h>
int binding_measure_views(BindingMeasureProc callback,void* user)
{
 BindingMeasureVec size={1.0f,2.0f};float baseline=3.0f;
 int status=callback(&size,&baseline,user);
 assert(!status && size.fX==10.0f && size.fY==20.0f && baseline==15.0f);
 return 0;
}
