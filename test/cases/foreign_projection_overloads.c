#include "foreign_projection_overloads.h"
int projectionScale(const ProjectionDescriptor* descriptor)
{return descriptor->value*(descriptor->scale ? descriptor->scale:1);}
int projectionStatus(const ProjectionDescriptor* descriptor)
{return descriptor->value>=0 ? 0:-1;}
int projectionSum(const ProjectionPair* pairs,int count)
{int result=0;for(int i=0;i<count;++i)result+=pairs[i].first+pairs[i].second;return result;}
