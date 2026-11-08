#ifndef XIDE_FILE_ASSOCIATION_H
#define XIDE_FILE_ASSOCIATION_H
#include <windows.h>
enum { XIDE_ASSOC_WORKSPACE=1,XIDE_ASSOC_PROJECT=2,XIDE_ASSOC_SCRIPT=4,XIDE_ASSOC_C=8 };
typedef struct xide_association_result { int registered[4],isDefault[4],defaultKnown[4]; DWORD error; } xide_association_result;
extern const wchar_t* const xideAssociationExtensions[4];
/* NULL prefix is the real per-user registration. Tests use an isolated HKCU
 * subtree; that mode never queries or notifies the system association service. */
DWORD xideAssociationProbe(const wchar_t* prefix,const wchar_t* exe,xide_association_result*);
DWORD xideAssociationRegister(const wchar_t* prefix,const wchar_t* exe,unsigned mask);
DWORD xideAssociationUnregister(const wchar_t* prefix,const wchar_t* exe,unsigned mask);
void xideAssociationOpenDefaults(void);
#endif
