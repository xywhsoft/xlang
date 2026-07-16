#include "../../extlibs/csv/include/xrt_csv.h"

#include <stdio.h>



int main(void)
{
	const str sHeader[] = { (str)"name", (str)"city", (str)"note" };
	const str sAlice[] = {
		(str)"Alice", (str)"\xe4\xb8\x8a\xe6\xb5\xb7", (str)"comma, quote \" and newline\nare safe"
	};
	xrtcsvwriter* pWriter;
	xrtcsvreader* pReader;
	xrtcsvrow tRow;
	xbuffer pData;
	int bOk = 1;
	if ( xrtInit() == NULL ) { return 1; }
	pWriter = xrtCsvWriterCreateMemory(NULL);
	bOk = pWriter != NULL && xrtCsvWriterWrite(pWriter, 3u, sHeader) &&
		xrtCsvWriterWrite(pWriter, 3u, sAlice) && xrtCsvWriterClose(pWriter);
	pData = bOk ? xrtCsvWriterTakeBuffer(pWriter) : NULL;
	xrtCsvWriterDestroy(pWriter);
	if ( pData == NULL ) { xrtUnit(); return 2; }

	pReader = xrtCsvReaderOpenMemory(pData->Buffer, pData->Length, NULL);
	bOk = xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 3u;
	bOk = bOk && xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 3u;
	if ( bOk ) {
		printf("%s lives in %s\n", tRow.pFields[0], tRow.pFields[1]);
	}
	bOk = bOk && !xrtCsvReaderNext(pReader, &tRow) && xrtCsvReaderEOF(pReader);
	xrtCsvReaderDestroy(pReader);
	xrtBufferDestroy(pData);
	xrtUnit();
	if ( bOk ) { puts("csv C example: PASS"); }
	return bOk ? 0 : 3;
}

/*
	CSV 是源码型扩展库。范例直接包含实现，确保 `xl run csv_c.c` 单文件可运行；
	正式项目通过 package 导入时，构建器会按清单分别编译这些源文件。
*/
#include "../../extlibs/csv/xrt_csv_common.c"
#include "../../extlibs/csv/xrt_csv_reader.c"
#include "../../extlibs/csv/xrt_csv_writer.c"
