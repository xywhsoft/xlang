#include "foreign_status_native.c"
#include "foreign_output_values.h"
int output_first(int* out,int code){*out=91;return code;}
int output_record(int value,SpanPoint* out){out->x=(float)value;out->y=7.0f;return 17;}
int output_span(const uint8_t* data,int size,int* out){*out=0;for(int i=0;i<size;++i)*out+=data[i];return 0;}
int output_root(ManagedRoot* root,int* out){assert(root && !root->closing);*out=root->value;return 0;}
int output_root_read(ManagedRoot* root,int* out){assert(root && !root->closing);*out=root->value;return 0;}
int output_root_span(ManagedRoot* root,const uint8_t* p,int size,int* out){assert(root && !root->closing);*out=0;for(int i=0;i<size;++i)*out+=p[i];return 0;}
