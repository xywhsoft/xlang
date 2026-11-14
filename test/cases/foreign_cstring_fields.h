#ifndef XL6_CSTRING_FIELDS_H
#define XL6_CSTRING_FIELDS_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct CStringFields {const char* text;char* mutableText;} CStringFields;
CStringFields* cstringFieldsGet(int);
void cstringFieldsChange(void);
int cstringFieldsWith(int,int (*)(const CStringFields*,void*),void*);
void cstringFieldsBudget(int);
void cstringFieldsBegin(void);
int cstringFieldsAllocations(void);
int cstringFieldsCalls(void);
#ifdef __cplusplus
}
#endif
#endif
