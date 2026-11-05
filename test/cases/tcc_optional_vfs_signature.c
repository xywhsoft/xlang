/* Private loader slots must exactly match the provider's public C ABI. */
#include "tcc_builtin_vfs.h"
#include "src/api/xl_api_internal.h"
#define EXACT(name,type) _Static_assert(_Generic(&(name),type:1,default:0),"optional VFS function ABI: " #name)
EXACT(tcc_vfs_open,XL_TccVfsOpenProc);
EXACT(tcc_vfs_close,XL_TccVfsCloseProc);
EXACT(tcc_vfs_read,XL_TccVfsReadProc);
EXACT(tcc_vfs_lseek,XL_TccVfsLseekProc);
EXACT(tcc_vfs_fopen,XL_TccVfsFopenProc);
EXACT(tcc_vfs_fclose,XL_TccVfsFcloseProc);
EXACT(tcc_vfs_mount_memory,XL_TccVfsMountMemoryProc);
EXACT(tcc_vfs_clear_dynamic,XL_TccVfsClearDynamicProc);
int main(void){return 0;}
