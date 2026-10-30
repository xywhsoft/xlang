/* 与独立 INI 实现互读互写，只使用双方共有的节、键和值语法。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"

#include <stdio.h>
#include <string.h>

static int emitConfig(void)
{
	xrtiniconfig* pConfig = xrtIniCreate(NULL);
	xbuffer* pText = NULL;
	int bOk = pConfig != NULL;
	if ( bOk ) {
		bOk = xrtIniSet(pConfig, (str)"server", (str)"port", (str)"9090") &&
			xrtIniSet(pConfig, (str)"server", (str)"name",
				(str)"\xe4\xbd\xa0\xe5\xa5\xbd") &&
			xrtIniSet(pConfig, (str)"client", (str)"timeout", (str)"5");
	}
	if ( bOk ) { pText = xrtIniStringify(pConfig); }
	bOk = bOk && pText != NULL &&
		fwrite(pText->Data, 1u, (size_t)pText->Size, stdout) == (size_t)pText->Size;
	xrtBufferDestroy(pText);
	if ( pConfig != NULL ) { xrtIniDestroy(pConfig); }
	return bOk ? 0 : 1;
}

static int readConfig(bool bStrict, const char* sExpectedName)
{
	char sInput[8192];
	size_t iSize = fread(sInput, 1u, sizeof(sInput), stdin);
	xrtiniconfig* pConfig;
	str sPort = NULL, sName = NULL, sTimeout = NULL;
	int bOk;
	if ( ferror(stdin) || (iSize == sizeof(sInput) && fgetc(stdin) != EOF) ) {
		return 1;
	}
	xrtinioptions Options;
	xrtIniOptionsInit(&Options);
	Options.bStrictDuplicates = bStrict;
	pConfig = xrtIniParseMemory(sInput, (uint64)iSize, &Options);
	if ( pConfig == NULL ) { return 1; }
	sPort = xrtIniGet(pConfig, (str)"server", (str)"port");
	sName = xrtIniGet(pConfig, (str)"server", (str)"name");
	sTimeout = xrtIniGet(pConfig, (str)"client", (str)"timeout");
	bOk = sPort != NULL && strcmp(sPort, "9090") == 0 &&
		sName != NULL && strcmp(sName, sExpectedName) == 0 &&
		sTimeout != NULL && strcmp(sTimeout, "5") == 0;
	xrtFree(sPort);
	xrtFree(sName);
	xrtFree(sTimeout);
	xrtIniDestroy(pConfig);
	return bOk ? 0 : 1;
}

int main(int argc, char** argv)
{
	if ( argc != 2 ) { return 2; }
	if ( strcmp(argv[1], "emit") == 0 ) { return emitConfig(); }
	if ( strcmp(argv[1], "read") == 0 ) {
		return readConfig(true, "\xe4\xbd\xa0\xe5\xa5\xbd");
	}
	if ( strcmp(argv[1], "read-updates") == 0 ) {
		return readConfig(false, "\xe4\xbd\xa0\xe5\xa5\xbd\nnext");
	}
	return 2;
}

/* 与包内 C 测试相同：单编译单元共享 XRT 模块配置。 */
#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
