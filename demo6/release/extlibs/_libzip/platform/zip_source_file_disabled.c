#include "../third_party/libzip/lib/zipint.h"

/* xlang/XRT 必须通过自定义 source 接入 UTF-8 文件系统和 VFS。 */
ZIP_EXTERN zip_source_t* zip_source_file_create(const char* sName, zip_uint64_t iStart,
	zip_int64_t iLength, zip_error_t* pError)
{
	(void)sName;
	(void)iStart;
	(void)iLength;
	zip_error_set(pError, ZIP_ER_OPNOTSUPP, 0);
	return NULL;
}

ZIP_EXTERN zip_source_t* zip_source_file(zip_t* pArchive, const char* sName,
	zip_uint64_t iStart, zip_int64_t iLength)
{
	if ( pArchive == NULL ) { return NULL; }
	return zip_source_file_create(sName, iStart, iLength, &pArchive->error);
}
