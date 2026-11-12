#ifndef XIDE_WORKSPACE_EDIT_PHYSICAL_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_PHYSICAL_INTERNAL_H
#include "platform/path.h"
#include <windows.h>
/* Shared ordinary-object identity/path rules; no allocation or source reads. */
static inline int xideEditPhysicalSame(const BY_HANDLE_FILE_INFORMATION* a,const BY_HANDLE_FILE_INFORMATION* b)
{return a->dwVolumeSerialNumber==b->dwVolumeSerialNumber && a->nFileIndexHigh==b->nFileIndexHigh && a->nFileIndexLow==b->nFileIndexLow;}
static inline int xideEditPhysicalLocalPath(const wchar_t* p)
{
 size_t n=wcslen(p);if(n<8 || wcsncmp(p,L"\\\\?\\",4) || !((p[4]>=L'A' && p[4]<=L'Z') || (p[4]>=L'a' && p[4]<=L'z')) || p[5]!=L':' || p[6]!=L'\\')return 0;
 wchar_t root[]={p[4],L':',L'\\',0};if(GetDriveTypeW(root)!=DRIVE_FIXED)return 0;
 size_t start=7;for(size_t i=7;i<=n;++i){if(!p[i] || p[i]==L'\\'){if(i==start || p[i-1]==L'.' || p[i-1]==L' ')return 0;start=i+1;}else if(p[i]<32 || wcschr(L"<>:\"/|?*",p[i]))return 0;}return 1;
}
static inline int xideEditPhysicalOrdinary(HANDLE h,int directory,BY_HANDLE_FILE_INFORMATION* info)
{return h!=INVALID_HANDLE_VALUE && GetFileType(h)==FILE_TYPE_DISK && GetFileInformationByHandle(h,info) && !(info->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) && !!(info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==directory;}
static inline int xideEditPhysicalWritableSingle(HANDLE h,int directory,BY_HANDLE_FILE_INFORMATION* info)
{return xideEditPhysicalOrdinary(h,directory,info) && (directory || (!(info->dwFileAttributes&FILE_ATTRIBUTE_READONLY) && info->nNumberOfLinks==1));}
#endif
