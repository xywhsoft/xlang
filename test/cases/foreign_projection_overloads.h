#ifndef XL6_TEST_PROJECTION_OVERLOADS_H
#define XL6_TEST_PROJECTION_OVERLOADS_H
typedef struct ProjectionPair { int first,second; } ProjectionPair;
#ifdef __cplusplus
static_assert(sizeof(int)==4,"fixture byte-vector uses the supported 32-bit C int ABI");
#else
_Static_assert(sizeof(int)==4,"fixture byte-vector uses the supported 32-bit C int ABI");
#endif
typedef struct ProjectionDescriptor { int value,scale; } ProjectionDescriptor;
typedef struct ProjectionOtherDescriptor { int value,scale; } ProjectionOtherDescriptor;
int projectionScale(const ProjectionDescriptor* descriptor);
int projectionStatus(const ProjectionDescriptor* descriptor);
int projectionSum(const ProjectionPair* pairs,int count);
#endif
