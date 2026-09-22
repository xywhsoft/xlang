/* 独立 C 测试和包导入使用相同的完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_csv.h"

#include <stdio.h>
#include <string.h>



typedef struct {
	const unsigned char* Data;
	uint64 Size;
	uint64 Position;
	uint64 Chunk;
} short_reader;



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_csv: %s\n", sMessage != NULL && sMessage[0] != 0 ?
		sMessage : "check failed");
	return 0;
}

static bool shortRead(ptr pContext, ptr pBuffer, uint64 iRequest, uint64* pRead)
{
	short_reader* pSource = (short_reader*)pContext;
	uint64 iRemain = pSource->Size - pSource->Position;
	uint64 iSize = iRequest;
	if ( iSize > pSource->Chunk ) { iSize = pSource->Chunk; }
	if ( iSize > iRemain ) { iSize = iRemain; }
	if ( iSize != 0u ) {
		memcpy(pBuffer, pSource->Data + pSource->Position, (size_t)iSize);
		pSource->Position += iSize;
	}
	*pRead = iSize;
	return true;
}

static bool shortClose(ptr pContext)
{
	(void)pContext;
	return true;
}

static int fieldEquals(const xrtcsvrow* pRow, uint32 iIndex, const char* sExpected)
{
	return pRow != NULL && iIndex < pRow->iCount &&
		strcmp(pRow->pFields[iIndex], sExpected) == 0;
}

static int testReader(void)
{
	static const unsigned char sCsv[] =
		"\xef\xbb\xbf" "name,note,empty\r\n"
		"\xe5\xbc\xa0\xe4\xb8\x89,\"a,b\",\"\"\r\n"
		"line,\"one\r\ntwo\",\"q\"\"q\"\r\n"
		",\r\n"
		"\r\n";
	short_reader tSource;
	xreaderops tOps;
	xrtcsvdialect tDialect;
	xrtcsvreader* pReader;
	xrtcsvrow tRow;
	int bOk = 1;
	memset(&tOps, 0, sizeof(tOps));
	tOps.Read = shortRead;
	tOps.Close = shortClose;
	tSource.Data = sCsv;
	tSource.Size = sizeof(sCsv) - 1u;
	tSource.Position = 0u;
	tSource.Chunk = 2u;
	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxInputBytes = sizeof(sCsv) - 1u;
	pReader = xrtCsvReaderCreate(xrtReaderCreate(&tOps, &tSource), true, &tDialect);

	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iRow == 0u &&
		tRow.iCount == 3u && fieldEquals(&tRow, 0u, "name") &&
		fieldEquals(&tRow, 1u, "note") && fieldEquals(&tRow, 2u, "empty"),
		"header row mismatch");
	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 3u &&
		fieldEquals(&tRow, 0u, "\xe5\xbc\xa0\xe4\xb8\x89") &&
		fieldEquals(&tRow, 1u, "a,b") && fieldEquals(&tRow, 2u, ""),
		"quoted UTF-8 row mismatch");
	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 3u &&
		fieldEquals(&tRow, 0u, "line") && fieldEquals(&tRow, 1u, "one\r\ntwo") &&
		fieldEquals(&tRow, 2u, "q\"q"), "embedded newline or quote mismatch");
	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 2u &&
		fieldEquals(&tRow, 0u, "") && fieldEquals(&tRow, 1u, ""),
		"empty field row mismatch");
	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 0u,
		"blank row must contain zero fields");
	bOk &= check(!xrtCsvReaderNext(pReader, &tRow) && xrtCsvReaderEOF(pReader),
		"CSV reader did not reach clean EOF");
	xrtCsvReaderDestroy(pReader);
	return bOk;
}

static int expectReaderError(const void* pData, uint64 iSize, xrtcsvdialect* pDialect,
	int iExpected, const char* sMessage)
{
	xrtcsvreader* pReader = xrtCsvReaderOpenMemory(pData, iSize, pDialect);
	xrtcsvrow tRow;
	int bOk = !xrtCsvReaderNext(pReader, &tRow) &&
		xrtCsvReaderError(pReader)->iCode == iExpected;
	xrtCsvReaderDestroy(pReader);
	return check(bOk, sMessage);
}

static int testReaderErrorsAndLimits(void)
{
	static const unsigned char sBadUtf8[] = { 'a', ',', 0xc3u, 0x28u, '\n' };
	xrtcsvdialect tDialect;
	int bOk = 1;
	xrtCsvDialectInit(&tDialect);
	bOk &= expectReaderError("\"abc", 4u, &tDialect, XRT_CSV_ERROR_FORMAT,
		"unterminated quote was accepted");
	bOk &= expectReaderError("a\"b,c\n", 6u, &tDialect, XRT_CSV_ERROR_FORMAT,
		"quote in an unquoted field was accepted");
	bOk &= expectReaderError("\"a\"x\n", 5u, &tDialect, XRT_CSV_ERROR_FORMAT,
		"suffix after a quoted field was accepted");
	bOk &= expectReaderError(sBadUtf8, sizeof(sBadUtf8), &tDialect, XRT_CSV_ERROR_FORMAT,
		"invalid UTF-8 was accepted");

	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxInputBytes = 3u;
	bOk &= expectReaderError("a,b\n", 4u, &tDialect, XRT_CSV_ERROR_LIMIT,
		"input byte limit was not enforced");
	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxItemBytes = 2u;
	bOk &= expectReaderError("abc\n", 4u, &tDialect, XRT_CSV_ERROR_LIMIT,
		"field byte limit was not enforced");
	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxOutputBytes = 2u;
	bOk &= expectReaderError("a,bc\n", 5u, &tDialect, XRT_CSV_ERROR_LIMIT,
		"decoded output limit was not enforced");
	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxNodes = 2u;
	bOk &= expectReaderError("a,b,c\n", 6u, &tDialect, XRT_CSV_ERROR_LIMIT,
		"field count limit was not enforced");
	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxEntries = 1u;
	{
		xrtcsvreader* pReader = xrtCsvReaderOpenMemory("a\nb\n", 4u, &tDialect);
		xrtcsvrow tRow;
		bOk &= check(xrtCsvReaderNext(pReader, &tRow) &&
			!xrtCsvReaderNext(pReader, &tRow) &&
			xrtCsvReaderError(pReader)->iCode == XRT_CSV_ERROR_LIMIT,
			"row count limit was not enforced");
		xrtCsvReaderDestroy(pReader);
	}
	return bOk;
}



static int testWriterRoundTrip(void)
{
	const str sFirst[] = { (str)"name", (str)"note", (str)"empty" };
	const str sSecond[] = { (str)"\xe5\xbc\xa0\xe4\xb8\x89", (str)"a,b", (str)"q\"q\nnext" };
	xrtcsvdialect tDialect;
	xrtcsvwriter* pWriter;
	xrtcsvreader* pReader;
	xrtcsvrow tRow;
	xbuffer* pData;
	int bOk = 1;
	xrtCsvDialectInit(&tDialect);
	tDialect.iNewline = XRT_CSV_NEWLINE_CRLF;
	tDialect.bWriteBOM = true;
	pWriter = xrtCsvWriterCreateMemory(&tDialect);
	bOk &= check(pWriter != NULL && xrtCsvWriterError(pWriter)->iCode == XRT_CSV_OK,
		"cannot create CSV memory writer");
	bOk &= check(xrtCsvWriterWrite(pWriter, 3u, sFirst),
		xrtCsvWriterError(pWriter)->sMessage);
	bOk &= check(xrtCsvWriterWrite(pWriter, 3u, sSecond),
		xrtCsvWriterError(pWriter)->sMessage);
	bOk &= check(xrtCsvWriterClose(pWriter), xrtCsvWriterError(pWriter)->sMessage);
	pData = xrtCsvWriterTakeBuffer(pWriter);
	bOk &= check(pData != NULL && pData->Size > 3u &&
		memcmp(pData->Data, "\xef\xbb\xbf", 3u) == 0,
		"CSV BOM was not written");
	xrtCsvWriterDestroy(pWriter);

	pReader = xrtCsvReaderOpenMemory(pData->Data, pData->Size, NULL);
	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 3u &&
		fieldEquals(&tRow, 0u, "name"), "written header cannot be parsed");
	bOk &= check(xrtCsvReaderNext(pReader, &tRow) && tRow.iCount == 3u &&
		fieldEquals(&tRow, 0u, "\xe5\xbc\xa0\xe4\xb8\x89") &&
		fieldEquals(&tRow, 1u, "a,b") && fieldEquals(&tRow, 2u, "q\"q\nnext"),
		"CSV writer round trip mismatch");
	bOk &= check(!xrtCsvReaderNext(pReader, &tRow) && xrtCsvReaderEOF(pReader),
		"round-trip reader did not reach EOF");
	xrtCsvReaderDestroy(pReader);
	xrtBufferDestroy(pData);
	return bOk;
}

static int testWriterOptionsAndLimits(void)
{
	const str sFields[] = { (str)"a", (str)"b,c" };
	xrtcsvdialect tDialect;
	xrtcsvwriter* pWriter;
	xbuffer* pData;
	int bOk = 1;
	xrtCsvDialectInit(&tDialect);
	tDialect.iDelimiter = ';';
	tDialect.bQuoteAll = true;
	pWriter = xrtCsvWriterCreateMemory(&tDialect);
	bOk &= check(xrtCsvWriterWrite(pWriter, 2u, sFields) && xrtCsvWriterClose(pWriter),
		xrtCsvWriterError(pWriter)->sMessage);
	pData = xrtCsvWriterTakeBuffer(pWriter);
	bOk &= check(pData != NULL && pData->Size == 10u &&
		memcmp(pData->Data, "\"a\";\"b,c\"\n", 10u) == 0,
		"custom CSV dialect output mismatch");
	xrtBufferDestroy(pData);
	xrtCsvWriterDestroy(pWriter);

	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxNodes = 2u;
	pWriter = xrtCsvWriterCreateMemory(&tDialect);
	bOk &= check(xrtCsvWriterWrite(pWriter, 2u, sFields) &&
		!xrtCsvWriterWrite(pWriter, 1u, sFields) &&
		xrtCsvWriterError(pWriter)->iCode == XRT_CSV_ERROR_LIMIT,
		"writer cumulative field limit was not enforced");
	xrtCsvWriterDestroy(pWriter);

	xrtCsvDialectInit(&tDialect);
	tDialect.tLimits.iMaxOutputBytes = 3u;
	pWriter = xrtCsvWriterCreateMemory(&tDialect);
	bOk &= check(!xrtCsvWriterWrite(pWriter, 2u, sFields) &&
		xrtCsvWriterError(pWriter)->iCode == XRT_CSV_ERROR_LIMIT,
		"writer output limit was not enforced");
	xrtCsvWriterDestroy(pWriter);
	return bOk;
}

static int testUtf8File(void)
{
	const str sPath = (str)"build/tmp/csv_\xe8\xb7\xaf\xe5\xbe\x84/\xe6\xb5\x8b\xe8\xaf\x95.csv";
	const str sFields[] = { (str)"key", (str)"value" };
	xrtcsvwriter* pWriter;
	xrtcsvreader* pReader;
	xrtcsvrow tRow;
	int bOk = 1;
	xrtDirCreateAll((str)"build/tmp/csv_\xe8\xb7\xaf\xe5\xbe\x84");
	pWriter = xrtCsvWriterOpenFile(sPath, NULL);
	bOk &= check(pWriter != NULL && xrtCsvWriterWrite(pWriter, 2u, sFields) &&
		xrtCsvWriterClose(pWriter), pWriter != NULL ?
		xrtCsvWriterError(pWriter)->sMessage : "cannot open UTF-8 CSV path");
	xrtCsvWriterDestroy(pWriter);
	pReader = xrtCsvReaderOpenFile(sPath, NULL);
	bOk &= check(pReader != NULL && xrtCsvReaderNext(pReader, &tRow) &&
		tRow.iCount == 2u && fieldEquals(&tRow, 1u, "value"),
		pReader != NULL ? xrtCsvReaderError(pReader)->sMessage : "cannot read UTF-8 CSV path");
	xrtCsvReaderDestroy(pReader);
	xrtDirRemoveAll("build/tmp/csv_\xe8\xb7\xaf\xe5\xbe\x84");
	return bOk;
}



int main(void)
{
	int bOk;
	bOk = testReader();
	bOk &= testReaderErrorsAndLimits();
	bOk &= testWriterRoundTrip();
	bOk &= testWriterOptionsAndLimits();
	bOk &= testUtf8File();
	if ( bOk ) { puts("test_csv: PASS"); }
	return bOk ? 0 : 1;
}

/* 单文件 C smoke：声明、实现与测试共享同一份 XRT 模块配置。 */
#include "../xrt_csv_common.c"
#include "../xrt_csv_reader.c"
#include "../xrt_csv_writer.c"
