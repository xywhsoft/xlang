#ifndef XRT_INTERNAL_FORM_DATA_H
#define XRT_INTERNAL_FORM_DATA_H

#include "../../include/xhttp_form_data_package.h"

#include <string.h>

#if defined(XHTTP_FEATURE_FORM_DATA)
#endif



#if defined(XHTTP_FEATURE_FORM_DATA)

/* 每个条目用一个连续副本保存全部文本元数据。 */
typedef struct xrt_form_data_entry {
	str Metadata;
	size_t NameSize;
	size_t FilenameSize;
	size_t ContentTypeSize;
	xhttpbody* Body;
	uint32 Flags;
} xrt_form_data_entry;



/* 容器维护逻辑正文长度，未知正文数量单独计数。 */
struct xformdata {
	xformdataconfig Config;
	xrt_form_data_entry* Entries;
	size_t Count;
	size_t Capacity;
	size_t Metadata;
	uint64 KnownBodyBytes;
	size_t UnknownBodies;
};



/* 返回条目借用的名称。 */
xstrview __xrtFormDataEntryName(
	const xrt_form_data_entry* pEntry
);



/* 返回条目借用的文件名；参数缺席时返回空视图。 */
xstrview __xrtFormDataEntryFilename(
	const xrt_form_data_entry* pEntry
);



/* 返回条目借用的媒体类型；字段缺席时返回空视图。 */
xstrview __xrtFormDataEntryContentType(
	const xrt_form_data_entry* pEntry
);



/* 发布 FormData 域错误。 */
bool __xrtFormDataFail(
	xerrkind Kind,
	xformdataerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 验证输出区间不会覆盖容器拥有的内部存储。 */
bool __xrtFormDataOutputValid(
	const xformdata* pForm,
	const void* pOutput,
	size_t iOutput
);

#endif

#endif
