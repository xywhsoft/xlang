/* INI 使用 IO/file，独立 C 测试显式请求完整 XRT 类型表。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"

#include <math.h>
#include <stdio.h>
#include <string.h>



static int check(int bCondition, const char* sMessage)
{
	if ( bCondition ) { return 1; }
	fprintf(stderr, "test_ini: %s\n", sMessage != NULL && sMessage[0] != 0 ?
		sMessage : "check failed");
	return 0;
}

static int textEquals(const char* sValue, const char* sExpected)
{
	return sValue != NULL && sExpected != NULL && strcmp(sValue, sExpected) == 0;
}

static int testParseAndModel(void)
{
	static const unsigned char sText[] =
		"\xef\xbb\xbf" "# leading comment\r\n"
		"global = root\r\n\r\n"
		"[server]\r\n"
		"host = 127.0.0.1\r\n"
		"port: 8080\r\n"
		"enabled = yes\r\n"
		"ratio = 1.25\r\n"
		"message = first line\r\n"
		"    second line\r\n"
		"; before name\r\n"
		"name = \xe6\xb5\x8b\xe8\xaf\x95\r\n";
	xrtiniconfig* pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, NULL);
	int64 iPort = 0;
	double fRatio = 0.0;
	bool bEnabled = false;
	xbuffer* pOutput;
	int bOk = 1;
	bOk &= check(pConfig != NULL && xrtIniError(pConfig)->iCode == XRT_INI_OK,
		pConfig != NULL ? xrtIniError(pConfig)->sMessage : "cannot create INI config");
	if ( pConfig == NULL || xrtIniError(pConfig)->iCode != XRT_INI_OK ) {
		xrtIniDestroy(pConfig);
		return 0;
	}
	bOk &= check(xrtIniSectionCount(pConfig) == 2u &&
		textEquals(xrtIniSectionName(pConfig, 0u), "") &&
		textEquals(xrtIniSectionName(pConfig, 1u), "server"),
		"INI section order mismatch");
	bOk &= check(xrtIniKeyCount(pConfig, (str)"server") == 6u &&
		textEquals(xrtIniKeyName(pConfig, (str)"server", 0u), "host"),
		"INI key order mismatch");
	bOk &= check(textEquals(xrtIniGetRaw(pConfig, (str)"", (str)"global"), "root"),
		"INI global value mismatch");
	bOk &= check(textEquals(xrtIniGetRaw(pConfig, (str)"server", (str)"message"),
		"first line\nsecond line"), "INI continuation mismatch");
	bOk &= check(xrtIniGetInt(pConfig, (str)"server", (str)"port", &iPort) && iPort == 8080,
		"INI int conversion mismatch");
	bOk &= check(xrtIniGetFloat(pConfig, (str)"server", (str)"ratio", &fRatio) &&
		fabs(fRatio - 1.25) < 0.000001, "INI float conversion mismatch");
	bOk &= check(xrtIniGetBool(pConfig, (str)"server", (str)"enabled", &bEnabled) && bEnabled,
		"INI bool conversion mismatch");

	bOk &= check(xrtIniSet(pConfig, (str)"server", (str)"port", (str)"9090") &&
		xrtIniSet(pConfig, (str)"client", (str)"timeout", (str)"30"),
		"INI set failed");
	bOk &= check(xrtIniRemove(pConfig, (str)"server", (str)"enabled") &&
		!xrtIniHas(pConfig, (str)"server", (str)"enabled"), "INI remove failed");
	pOutput = xrtIniStringify(pConfig);
	{
		str sOutput = pOutput != NULL ?
			xrtStrDupN((const char*)pOutput->Data, pOutput->Size) : NULL;
		bOk &= check(sOutput != NULL &&
			strstr((const char*)sOutput, "# leading comment\n") != NULL &&
			strstr((const char*)sOutput, "; before name\nname = \xe6\xb5\x8b\xe8\xaf\x95\n") != NULL &&
			strstr((const char*)sOutput, "message = first line\n    second line\n") != NULL,
			"INI comments or multiline output were not preserved");
		xrtFree(sOutput);
	}
	xrtBufferDestroy(pOutput);
	bOk &= check(xrtIniRemoveSection(pConfig, (str)"client") &&
		!xrtIniHasSection(pConfig, (str)"client"), "INI section removal failed");
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testInterpolation(void)
{
	const char sText[] =
		"[server]\n"
		"host = localhost\n"
		"port = 8080\n"
		"url = http://${host}:${server.port}/api\n";
	const char sCycle[] = "[x]\na = ${x.b}\nb = ${x.a}\n";
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	str sValue;
	int bOk = 1;
	xrtIniOptionsInit(&tOptions);
	tOptions.bInterpolation = true;
	pConfig = xrtIniParseMemory(sText, sizeof(sText) - 1u, &tOptions);
	sValue = xrtIniGet(pConfig, (str)"server", (str)"url");
	bOk &= check(sValue != NULL && strcmp((const char*)sValue,
		"http://localhost:8080/api") == 0, "INI interpolation mismatch");
	xrtFree(sValue);
	xrtIniDestroy(pConfig);

	pConfig = xrtIniParseMemory(sCycle, sizeof(sCycle) - 1u, &tOptions);
	sValue = xrtIniGet(pConfig, (str)"x", (str)"a");
	bOk &= check(sValue == NULL && xrtIniError(pConfig)->iCode == XRT_INI_ERROR_INTERPOLATION,
		"INI interpolation cycle was accepted");
	xrtFree(sValue);
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testErrorsAndLimits(void)
{
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	int bOk = 1;
	{
		const char sDuplicate[] = "[x]\na=1\na=2\n";
		pConfig = xrtIniParseMemory(sDuplicate, sizeof(sDuplicate) - 1u, NULL);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_DUPLICATE,
		"duplicate INI key was accepted");
	xrtIniDestroy(pConfig);
	{
		const char sMalformed[] = "[x\na=1\n";
		pConfig = xrtIniParseMemory(sMalformed, sizeof(sMalformed) - 1u, NULL);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_FORMAT,
		"malformed INI section was accepted");
	xrtIniDestroy(pConfig);

	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxInputBytes = 3u;
	{
		const char sLimited[] = "a=123\n";
		pConfig = xrtIniParseMemory(sLimited, sizeof(sLimited) - 1u, &tOptions);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI input limit was not enforced");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxItemBytes = 2u;
	{
		const char sItem[] = "abc=1\n";
		pConfig = xrtIniParseMemory(sItem, sizeof(sItem) - 1u, &tOptions);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI item limit was not enforced");
	xrtIniDestroy(pConfig);
	xrtIniOptionsInit(&tOptions);
	tOptions.tLimits.iMaxNodes = 2u;
	{
		const char sNodes[] = "[x]\na=1\nb=2\n";
		pConfig = xrtIniParseMemory(sNodes, sizeof(sNodes) - 1u, &tOptions);
	}
	bOk &= check(xrtIniError(pConfig)->iCode == XRT_INI_ERROR_LIMIT,
		"INI node limit was not enforced");
	xrtIniDestroy(pConfig);
	return bOk;
}

static int testCaseAndFile(void)
{
	const str sPath = (str)"build/tmp/ini_\xe8\xb7\xaf\xe5\xbe\x84/\xe9\x85\x8d\xe7\xbd\xae.ini";
	xrtinioptions tOptions;
	xrtiniconfig* pConfig;
	xrtiniconfig* pLoaded;
	int bOk = 1;
	xrtIniOptionsInit(&tOptions);
	tOptions.bCaseSensitive = false;
	pConfig = xrtIniParseMemory("[Main]\nName=Value\n", 18u, &tOptions);
	bOk &= check(xrtIniHas(pConfig, (str)"main", (str)"name"),
		"case-insensitive INI lookup failed");
	xrtDirCreateAll((str)"build/tmp/ini_\xe8\xb7\xaf\xe5\xbe\x84");
	bOk &= check(xrtIniSave(pConfig, sPath), xrtIniError(pConfig)->sMessage);
	pLoaded = xrtIniLoad(sPath, &tOptions);
	bOk &= check(pLoaded != NULL && xrtIniHas(pLoaded, (str)"MAIN", (str)"NAME"),
		"UTF-8 INI path round trip failed");
	xrtIniDestroy(pLoaded);
	xrtIniDestroy(pConfig);
	xrtDirRemoveAll("build/tmp/ini_\xe8\xb7\xaf\xe5\xbe\x84");
	return bOk;
}



int main(void)
{
	int bOk;
	bOk = testParseAndModel();
	bOk &= testInterpolation();
	bOk &= testErrorsAndLimits();
	bOk &= testCaseAndFile();
	if ( bOk ) { puts("test_ini: PASS"); }
	return bOk ? 0 : 1;
}

/* 单文件 C smoke：声明、实现与测试共享同一份 XRT 模块配置。 */
#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
