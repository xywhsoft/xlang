/**
 * xlang - xrt 函数注册到 TCC
 * 
 * 将 xrt 运行时库的所有函数注册到 TCC 状态机中，
 * 使得 TCC 运行的代码可以直接调用这些函数。
 */



void xlImportXRT(TCCState* s)
{
	// 添加函数 - Core
	tcc_add_symbol(s, "xrtInit", xrtInit);
	tcc_add_symbol(s, "xrtUnit", xrtUnit);
	
	// 添加函数 - Sup
	#if defined(_WIN32) || defined(_WIN64)
		tcc_add_symbol(s, "memmem", memmem);
	#endif
	tcc_add_symbol(s, "u16len", u16len);
	tcc_add_symbol(s, "u32len", u32len);
	
	// 添加函数 - Base
	tcc_add_symbol(s, "xrtMalloc", xrtMalloc);
	tcc_add_symbol(s, "xrtCalloc", xrtCalloc);
	tcc_add_symbol(s, "xrtRealloc", xrtRealloc);
	tcc_add_symbol(s, "xrtFree", xrtFree);
	tcc_add_symbol(s, "xrtTempMemory", xrtTempMemory);
	tcc_add_symbol(s, "xrtFreeTempMemory", xrtFreeTempMemory);
	tcc_add_symbol(s, "xrtSetError", xrtSetError);
	tcc_add_symbol(s, "xrtSetErrorU16", xrtSetErrorU16);
	tcc_add_symbol(s, "xrtSetErrorU32", xrtSetErrorU32);
	tcc_add_symbol(s, "xrtClearError", xrtClearError);
	
	// 添加函数 - Charset
	tcc_add_symbol(s, "xrtUTF8to16", xrtUTF8to16);
	tcc_add_symbol(s, "xrtUTF8to32", xrtUTF8to32);
	tcc_add_symbol(s, "xrtUTF16to8", xrtUTF16to8);
	tcc_add_symbol(s, "xrtUTF16to32", xrtUTF16to32);
	tcc_add_symbol(s, "xrtUTF32to8", xrtUTF32to8);
	tcc_add_symbol(s, "xrtUTF32to16", xrtUTF32to16);
	tcc_add_symbol(s, "xrtUTF16LEtoBE", xrtUTF16LEtoBE);
	tcc_add_symbol(s, "xrtUTF32LEtoBE", xrtUTF32LEtoBE);
	tcc_add_symbol(s, "xrtConvCharset", xrtConvCharset);
	tcc_add_symbol(s, "xrtIsUTF8", xrtIsUTF8);
	tcc_add_symbol(s, "xrtDetectCharset", xrtDetectCharset);
	tcc_add_symbol(s, "xrtGetCharSize", xrtGetCharSize);
	
	// 添加函数 - OS
	tcc_add_symbol(s, "xrtRun", xrtRun);
	tcc_add_symbol(s, "xrtStart", xrtStart);
	tcc_add_symbol(s, "xrtChain", xrtChain);
	
	// 添加函数 - Math
	tcc_add_symbol(s, "xrtRandSeed", xrtRandSeed);
	tcc_add_symbol(s, "xrtRand32Ex", xrtRand32Ex);
	tcc_add_symbol(s, "xrtRand64Ex", xrtRand64Ex);
	tcc_add_symbol(s, "xrtRandRangeEx", xrtRandRangeEx);
	tcc_add_symbol(s, "xrtRand32", xrtRand32);
	tcc_add_symbol(s, "xrtRand64", xrtRand64);
	tcc_add_symbol(s, "xrtRandRange", xrtRandRange);
	tcc_add_symbol(s, "xrtIntApprox", xrtIntApprox);
	tcc_add_symbol(s, "xrtNumApprox", xrtNumApprox);
	
	// 添加函数 - String
	tcc_add_symbol(s, "xrtCopyStr", xrtCopyStr);
	tcc_add_symbol(s, "xrtCopyStrU16", xrtCopyStrU16);
	tcc_add_symbol(s, "xrtCopyStrU32", xrtCopyStrU32);
	tcc_add_symbol(s, "xrtCopyMem", xrtCopyMem);
	tcc_add_symbol(s, "xrtStrComp", xrtStrComp);
	tcc_add_symbol(s, "xrtLCase", xrtLCase);
	tcc_add_symbol(s, "xrtUCase", xrtUCase);
	tcc_add_symbol(s, "xrtFindStr", xrtFindStr);
	tcc_add_symbol(s, "xrtInStr", xrtInStr);
	tcc_add_symbol(s, "xrtCheckStr", xrtCheckStr);
	tcc_add_symbol(s, "xrtLTrim", xrtLTrim);
	tcc_add_symbol(s, "xrtRTrim", xrtRTrim);
	tcc_add_symbol(s, "xrtTrim", xrtTrim);
	tcc_add_symbol(s, "xrtFilterStr", xrtFilterStr);
	tcc_add_symbol(s, "xrtFormat", xrtFormat);
	tcc_add_symbol(s, "xrtReplace", xrtReplace);
	tcc_add_symbol(s, "xrtSplit", xrtSplit);
	tcc_add_symbol(s, "xrtRandStr", xrtRandStr);
	tcc_add_symbol(s, "xrtHexEncode", xrtHexEncode);
	tcc_add_symbol(s, "xrtHexDecode", xrtHexDecode);
	tcc_add_symbol(s, "xrtBase64Encode", xrtBase64Encode);
	tcc_add_symbol(s, "xrtBase64Decode", xrtBase64Decode);
	tcc_add_symbol(s, "xrtStrLike", xrtStrLike);
	tcc_add_symbol(s, "xrtIntFormat", xrtIntFormat);
	tcc_add_symbol(s, "xrtNumFormat", xrtNumFormat);
	tcc_add_symbol(s, "xrtStrSim", xrtStrSim);
	tcc_add_symbol(s, "xrtStrApprox", xrtStrApprox);
	
	// 添加函数 - Path
	tcc_add_symbol(s, "xrtPathGetNameExt", xrtPathGetNameExt);
	tcc_add_symbol(s, "xrtPathGetName", xrtPathGetName);
	tcc_add_symbol(s, "xrtPathGetExt", xrtPathGetExt);
	tcc_add_symbol(s, "xrtPathGetDir", xrtPathGetDir);
	tcc_add_symbol(s, "xrtPathIsAbs", xrtPathIsAbs);
	tcc_add_symbol(s, "xrtPathRandom", xrtPathRandom);
	tcc_add_symbol(s, "xrtPathJoin", xrtPathJoin);
	
	// 添加函数 - Time
	tcc_add_symbol(s, "xrtTimer", xrtTimer);
	tcc_add_symbol(s, "xrtSleep", xrtSleep);
	tcc_add_symbol(s, "xrtIsLeapYear", xrtIsLeapYear);
	tcc_add_symbol(s, "xrtDaysInMonth", xrtDaysInMonth);
	tcc_add_symbol(s, "xrtDaysInYear", xrtDaysInYear);
	tcc_add_symbol(s, "xrtTimeSerial", xrtTimeSerial);
	tcc_add_symbol(s, "xrtDateSerial", xrtDateSerial);
	tcc_add_symbol(s, "xrtDateTimeSerial", xrtDateTimeSerial);
	tcc_add_symbol(s, "xrtSecond", xrtSecond);
	tcc_add_symbol(s, "xrtMinute", xrtMinute);
	tcc_add_symbol(s, "xrtHour", xrtHour);
	tcc_add_symbol(s, "xrtDay", xrtDay);
	tcc_add_symbol(s, "xrtMonth", xrtMonth);
	tcc_add_symbol(s, "xrtYear", xrtYear);
	tcc_add_symbol(s, "xrtWeekday", xrtWeekday);
	tcc_add_symbol(s, "xrtDayOfYear", xrtDayOfYear);
	tcc_add_symbol(s, "xrtDecodeSerial", xrtDecodeSerial);
	tcc_add_symbol(s, "xrtNow", xrtNow);
	tcc_add_symbol(s, "xrtDate", xrtDate);
	tcc_add_symbol(s, "xrtTime", xrtTime);
	tcc_add_symbol(s, "xrtNowStr", xrtNowStr);
	tcc_add_symbol(s, "xrtDateStr", xrtDateStr);
	tcc_add_symbol(s, "xrtTimeStr", xrtTimeStr);
	tcc_add_symbol(s, "xrtTimeToStr", xrtTimeToStr);
	tcc_add_symbol(s, "xrtDateAdd", xrtDateAdd);
	tcc_add_symbol(s, "xrtDateDiff", xrtDateDiff);
	tcc_add_symbol(s, "xrtStrToTime", xrtStrToTime);
	tcc_add_symbol(s, "xrtQuarter", xrtQuarter);
	tcc_add_symbol(s, "xrtDatePart", xrtDatePart);
	tcc_add_symbol(s, "xrtTimePart", xrtTimePart);
	tcc_add_symbol(s, "xrtIsSameDay", xrtIsSameDay);
	tcc_add_symbol(s, "xrtIsSameMonth", xrtIsSameMonth);
	tcc_add_symbol(s, "xrtIsSameYear", xrtIsSameYear);
	tcc_add_symbol(s, "xrtTimeInRange", xrtTimeInRange);
	tcc_add_symbol(s, "xrtTimeRangeOverlap", xrtTimeRangeOverlap);
	tcc_add_symbol(s, "xrtToUnixTime", xrtToUnixTime);
	tcc_add_symbol(s, "xrtFromUnixTime", xrtFromUnixTime);
	tcc_add_symbol(s, "xrtFirstDayOfMonth", xrtFirstDayOfMonth);
	tcc_add_symbol(s, "xrtLastDayOfMonth", xrtLastDayOfMonth);
	tcc_add_symbol(s, "xrtFirstDayOfYear", xrtFirstDayOfYear);
	tcc_add_symbol(s, "xrtLastDayOfYear", xrtLastDayOfYear);
	tcc_add_symbol(s, "xrtFirstDayOfWeek", xrtFirstDayOfWeek);
	tcc_add_symbol(s, "xrtLastDayOfWeek", xrtLastDayOfWeek);
	tcc_add_symbol(s, "xrtWeekOfYear", xrtWeekOfYear);
	tcc_add_symbol(s, "xrtWeekOfMonth", xrtWeekOfMonth);
	tcc_add_symbol(s, "xrtNowUTC", xrtNowUTC);
	tcc_add_symbol(s, "xrtTimezoneOffset", xrtTimezoneOffset);
	tcc_add_symbol(s, "xrtUTCToLocal", xrtUTCToLocal);
	tcc_add_symbol(s, "xrtLocalToUTC", xrtLocalToUTC);
	tcc_add_symbol(s, "xrtRelativeTime", xrtRelativeTime);
	tcc_add_symbol(s, "xrtTimeFormat", xrtTimeFormat);
	tcc_add_symbol(s, "xrtTimeParse", xrtTimeParse);
	tcc_add_symbol(s, "xrtTimeApprox", xrtTimeApprox);
	
	// 添加函数 - File
	tcc_add_symbol(s, "xrtOpen", xrtOpen);
	tcc_add_symbol(s, "xrtClose", xrtClose);
	tcc_add_symbol(s, "xrtSeek", xrtSeek);
	tcc_add_symbol(s, "xrtTell", xrtTell);
	tcc_add_symbol(s, "xrtGetEOF", xrtGetEOF);
	tcc_add_symbol(s, "xrtEOF", xrtEOF);
	tcc_add_symbol(s, "xrtSetEOF", xrtSetEOF);
	tcc_add_symbol(s, "xrtRead", xrtRead);
	tcc_add_symbol(s, "xrtWrite", xrtWrite);
	tcc_add_symbol(s, "xrtGet", xrtGet);
	tcc_add_symbol(s, "xrtPut", xrtPut);
	tcc_add_symbol(s, "xrtFileAppend", xrtFileAppend);
	tcc_add_symbol(s, "xrtFileWriteAll", xrtFileWriteAll);
	tcc_add_symbol(s, "xrtFileReadAll", xrtFileReadAll);
	tcc_add_symbol(s, "xrtFilePutAll", xrtFilePutAll);
	tcc_add_symbol(s, "xrtFileGetAll", xrtFileGetAll);
	tcc_add_symbol(s, "xrtPathExists", xrtPathExists);
	tcc_add_symbol(s, "xrtFileExists", xrtFileExists);
	tcc_add_symbol(s, "xrtDirExists", xrtDirExists);
	tcc_add_symbol(s, "xrtFileGetSize", xrtFileGetSize);
	tcc_add_symbol(s, "xrtFileSetSize", xrtFileSetSize);
	tcc_add_symbol(s, "xrtFileGetAttr", xrtFileGetAttr);
	tcc_add_symbol(s, "xrtFileSetAttr", xrtFileSetAttr);
	tcc_add_symbol(s, "xrtFileGetAccessTime", xrtFileGetAccessTime);
	tcc_add_symbol(s, "xrtFileGetChangeTime", xrtFileGetChangeTime);
	tcc_add_symbol(s, "xrtFileCopy", xrtFileCopy);
	tcc_add_symbol(s, "xrtFileMove", xrtFileMove);
	tcc_add_symbol(s, "xrtFileDelete", xrtFileDelete);
	tcc_add_symbol(s, "xrtDirScan", xrtDirScan);
	tcc_add_symbol(s, "xrtDirCreate", xrtDirCreate);
	tcc_add_symbol(s, "xrtDirCreateAll", xrtDirCreateAll);
	tcc_add_symbol(s, "xrtDirCopy", xrtDirCopy);
	tcc_add_symbol(s, "xrtDirMove", xrtDirMove);
	tcc_add_symbol(s, "xrtDirDelete", xrtDirDelete);
	
	// 添加函数 - Thread
	tcc_add_symbol(s, "xrtThreadCreate", xrtThreadCreate);
	tcc_add_symbol(s, "xrtThreadDestroy", xrtThreadDestroy);
	tcc_add_symbol(s, "xrtThreadWait", xrtThreadWait);
	tcc_add_symbol(s, "xrtThreadWaitTimeout", xrtThreadWaitTimeout);
	tcc_add_symbol(s, "xrtThreadStop", xrtThreadStop);
	tcc_add_symbol(s, "xrtThreadShouldStop", xrtThreadShouldStop);
	tcc_add_symbol(s, "xrtThreadKill", xrtThreadKill);
	tcc_add_symbol(s, "xrtThreadSuspend", xrtThreadSuspend);
	tcc_add_symbol(s, "xrtThreadResume", xrtThreadResume);
	tcc_add_symbol(s, "xrtThreadGetState", xrtThreadGetState);
	tcc_add_symbol(s, "xrtThreadGetExitCode", xrtThreadGetExitCode);
	tcc_add_symbol(s, "xrtThreadGetCurrentId", xrtThreadGetCurrentId);
	tcc_add_symbol(s, "xrtThreadYield", xrtThreadYield);
	tcc_add_symbol(s, "xrtMutexCreate", xrtMutexCreate);
	tcc_add_symbol(s, "xrtMutexDestroy", xrtMutexDestroy);
	tcc_add_symbol(s, "xrtMutexInit", xrtMutexInit);
	tcc_add_symbol(s, "xrtMutexUnit", xrtMutexUnit);
	tcc_add_symbol(s, "xrtMutexLock", xrtMutexLock);
	tcc_add_symbol(s, "xrtMutexTryLock", xrtMutexTryLock);
	tcc_add_symbol(s, "xrtMutexUnlock", xrtMutexUnlock);
	tcc_add_symbol(s, "xrtSemCreate", xrtSemCreate);
	tcc_add_symbol(s, "xrtSemDestroy", xrtSemDestroy);
	tcc_add_symbol(s, "xrtSemInit", xrtSemInit);
	tcc_add_symbol(s, "xrtSemUnit", xrtSemUnit);
	tcc_add_symbol(s, "xrtSemWait", xrtSemWait);
	tcc_add_symbol(s, "xrtSemTryWait", xrtSemTryWait);
	tcc_add_symbol(s, "xrtSemWaitTimeout", xrtSemWaitTimeout);
	tcc_add_symbol(s, "xrtSemPost", xrtSemPost);
	tcc_add_symbol(s, "xrtSemPostMultiple", xrtSemPostMultiple);
	tcc_add_symbol(s, "xrtCondCreate", xrtCondCreate);
	tcc_add_symbol(s, "xrtCondDestroy", xrtCondDestroy);
	tcc_add_symbol(s, "xrtCondInit", xrtCondInit);
	tcc_add_symbol(s, "xrtCondUnit", xrtCondUnit);
	tcc_add_symbol(s, "xrtCondWait", xrtCondWait);
	tcc_add_symbol(s, "xrtCondWaitTimeout", xrtCondWaitTimeout);
	tcc_add_symbol(s, "xrtCondSignal", xrtCondSignal);
	tcc_add_symbol(s, "xrtCondBroadcast", xrtCondBroadcast);
	
	// 添加函数 - Hash
	tcc_add_symbol(s, "xrtHash32_WithSeed", xrtHash32_WithSeed);
	tcc_add_symbol(s, "xrtHash32", xrtHash32);
	tcc_add_symbol(s, "xrtHash64_WithSeed", xrtHash64_WithSeed);
	tcc_add_symbol(s, "xrtHash64", xrtHash64);
	tcc_add_symbol(s, "xrtHash64_Micro_WithSeed", xrtHash64_Micro_WithSeed);
	tcc_add_symbol(s, "xrtHash64_Micro", xrtHash64_Micro);
	tcc_add_symbol(s, "xrtHash64_Nano_WithSeed", xrtHash64_Nano_WithSeed);
	tcc_add_symbol(s, "xrtHash64_Nano", xrtHash64_Nano);
	
	// 添加函数 - Network
	tcc_add_symbol(s, "xrtGetLocalIP", xrtGetLocalIP);
	tcc_add_symbol(s, "xrtGetLocalRawIP", xrtGetLocalRawIP);
	tcc_add_symbol(s, "xrtGetLocalMAC", xrtGetLocalMAC);
	tcc_add_symbol(s, "xrtGetLocalName", xrtGetLocalName);
	
	// 添加函数 - XID
	tcc_add_symbol(s, "xrtEncodeXID", xrtEncodeXID);
	tcc_add_symbol(s, "xrtDecodeXID", xrtDecodeXID);
	tcc_add_symbol(s, "xrtMakeXID", xrtMakeXID);
	tcc_add_symbol(s, "xrtMakeXIDS", xrtMakeXIDS);
	tcc_add_symbol(s, "xrtCompXID", xrtCompXID);
	
	// 添加函数 - Buffer
	tcc_add_symbol(s, "xrtBufferCreate", xrtBufferCreate);
	tcc_add_symbol(s, "xrtBufferDestroy", xrtBufferDestroy);
	tcc_add_symbol(s, "xrtBufferInit", xrtBufferInit);
	tcc_add_symbol(s, "xrtBufferUnit", xrtBufferUnit);
	tcc_add_symbol(s, "xrtBufferMalloc", xrtBufferMalloc);
	tcc_add_symbol(s, "xrtBufferInsert", xrtBufferInsert);
	tcc_add_symbol(s, "xrtBufferAppend", xrtBufferAppend);
	
	// 添加函数 - Point Array
	tcc_add_symbol(s, "xrtPtrArrayCreate", xrtPtrArrayCreate);
	tcc_add_symbol(s, "xrtPtrArrayDestroy", xrtPtrArrayDestroy);
	tcc_add_symbol(s, "xrtPtrArrayInit", xrtPtrArrayInit);
	tcc_add_symbol(s, "xrtPtrArrayUnit", xrtPtrArrayUnit);
	tcc_add_symbol(s, "xrtPtrArrayMalloc", xrtPtrArrayMalloc);
	tcc_add_symbol(s, "xrtPtrArrayInsert", xrtPtrArrayInsert);
	tcc_add_symbol(s, "xrtPtrArrayAppend", xrtPtrArrayAppend);
	tcc_add_symbol(s, "xrtPtrArrayAddAlt", xrtPtrArrayAddAlt);
	tcc_add_symbol(s, "xrtPtrArraySwap", xrtPtrArraySwap);
	tcc_add_symbol(s, "xrtPtrArrayRemove", xrtPtrArrayRemove);
	tcc_add_symbol(s, "xrtPtrArrayGet", xrtPtrArrayGet);
	tcc_add_symbol(s, "xrtPtrArrayGet_Unsafe", xrtPtrArrayGet_Unsafe);
	tcc_add_symbol(s, "xrtPtrArraySet", xrtPtrArraySet);
	tcc_add_symbol(s, "xrtPtrArraySet_Unsafe", xrtPtrArraySet_Unsafe);
	tcc_add_symbol(s, "xrtPtrArraySort", xrtPtrArraySort);
	
	// 添加函数 - Array
	tcc_add_symbol(s, "xrtArrayCreate", xrtArrayCreate);
	tcc_add_symbol(s, "xrtArrayDestroy", xrtArrayDestroy);
	tcc_add_symbol(s, "xrtArrayInit", xrtArrayInit);
	tcc_add_symbol(s, "xrtArrayUnit", xrtArrayUnit);
	tcc_add_symbol(s, "xrtArrayAlloc", xrtArrayAlloc);
	tcc_add_symbol(s, "xrtArrayInsert", xrtArrayInsert);
	tcc_add_symbol(s, "xrtArrayAppend", xrtArrayAppend);
	tcc_add_symbol(s, "xrtArraySwap", xrtArraySwap);
	tcc_add_symbol(s, "xrtArrayRemove", xrtArrayRemove);
	tcc_add_symbol(s, "xrtArrayGet", xrtArrayGet);
	tcc_add_symbol(s, "xrtArrayGet_Unsafe", xrtArrayGet_Unsafe);
	tcc_add_symbol(s, "xrtArraySort", xrtArraySort);
	
	// 添加函数 - BSMM
	tcc_add_symbol(s, "xrtBsmmCreate", xrtBsmmCreate);
	tcc_add_symbol(s, "xrtBsmmDestroy", xrtBsmmDestroy);
	tcc_add_symbol(s, "xrtBsmmInit", xrtBsmmInit);
	tcc_add_symbol(s, "xrtBsmmUnit", xrtBsmmUnit);
	tcc_add_symbol(s, "xrtBsmmAlloc", xrtBsmmAlloc);
	tcc_add_symbol(s, "xrtBsmmFree", xrtBsmmFree);
	
	// 添加函数 - Memory Unit
	tcc_add_symbol(s, "xrtMemUnitCreate", xrtMemUnitCreate);
	tcc_add_symbol(s, "xrtMemUnitAlloc", xrtMemUnitAlloc);
	tcc_add_symbol(s, "xrtMemUnitFreeIdx", xrtMemUnitFreeIdx);
	tcc_add_symbol(s, "xrtMemUnitFree", xrtMemUnitFree);
	tcc_add_symbol(s, "xrtMemUnitGC", xrtMemUnitGC);
	
	// 添加函数 - Fixed-Size Memory Pool
	tcc_add_symbol(s, "xrtFSMemPoolCreate", xrtFSMemPoolCreate);
	tcc_add_symbol(s, "xrtFSMemPoolDestroy", xrtFSMemPoolDestroy);
	tcc_add_symbol(s, "xrtFSMemPoolInit", xrtFSMemPoolInit);
	tcc_add_symbol(s, "xrtFSMemPoolUnit", xrtFSMemPoolUnit);
	tcc_add_symbol(s, "xrtFSMemPoolAlloc", xrtFSMemPoolAlloc);
	tcc_add_symbol(s, "xrtFSMemPoolFree", xrtFSMemPoolFree);
	tcc_add_symbol(s, "xrtFSMemPoolGC", xrtFSMemPoolGC);
	
	// 添加函数 - Stack
	tcc_add_symbol(s, "xrtStackCreate", xrtStackCreate);
	tcc_add_symbol(s, "xrtStackPush", xrtStackPush);
	tcc_add_symbol(s, "xrtStackPushData", xrtStackPushData);
	tcc_add_symbol(s, "xrtStackPushPtr", xrtStackPushPtr);
	tcc_add_symbol(s, "xrtStackPop", xrtStackPop);
	tcc_add_symbol(s, "xrtStackPopPtr", xrtStackPopPtr);
	tcc_add_symbol(s, "xrtStackTop", xrtStackTop);
	tcc_add_symbol(s, "xrtStackTopPtr", xrtStackTopPtr);
	tcc_add_symbol(s, "xrtStackGetPos", xrtStackGetPos);
	tcc_add_symbol(s, "xrtStackGetPos_Unsafe", xrtStackGetPos_Unsafe);
	tcc_add_symbol(s, "xrtStackGetPosPtr", xrtStackGetPosPtr);
	tcc_add_symbol(s, "xrtStackGetPosPtr_Unsafe", xrtStackGetPosPtr_Unsafe);
	
	// 添加函数 - Dynamic Stack
	tcc_add_symbol(s, "xrtDynStackCreate", xrtDynStackCreate);
	tcc_add_symbol(s, "xrtDynStackDestroy", xrtDynStackDestroy);
	tcc_add_symbol(s, "xrtDynStackInit", xrtDynStackInit);
	tcc_add_symbol(s, "xrtDynStackUnit", xrtDynStackUnit);
	tcc_add_symbol(s, "xrtDynStackPush", xrtDynStackPush);
	tcc_add_symbol(s, "xrtDynStackPushData", xrtDynStackPushData);
	tcc_add_symbol(s, "xrtDynStackPushPtr", xrtDynStackPushPtr);
	tcc_add_symbol(s, "xrtDynStackPop", xrtDynStackPop);
	tcc_add_symbol(s, "xrtDynStackPopPtr", xrtDynStackPopPtr);
	tcc_add_symbol(s, "xrtDynStackTop", xrtDynStackTop);
	tcc_add_symbol(s, "xrtDynStackTopPtr", xrtDynStackTopPtr);
	tcc_add_symbol(s, "xrtDynStackGetPos", xrtDynStackGetPos);
	tcc_add_symbol(s, "xrtDynStackGetPos_Unsafe", xrtDynStackGetPos_Unsafe);
	tcc_add_symbol(s, "xrtDynStackGetPosPtr", xrtDynStackGetPosPtr);
	tcc_add_symbol(s, "xrtDynStackGetPosPtr_Unsafe", xrtDynStackGetPosPtr_Unsafe);
	
	// 添加函数 - AVLTree Base
	tcc_add_symbol(s, "xrtAVLTB_Insert", xrtAVLTB_Insert);
	tcc_add_symbol(s, "xrtAVLTB_Remove", xrtAVLTB_Remove);
	tcc_add_symbol(s, "xrtAVLTB_Search", xrtAVLTB_Search);
	tcc_add_symbol(s, "xrtAVLTB_WalkRecuProc", xrtAVLTB_WalkRecuProc);
	tcc_add_symbol(s, "xrtAVLTB_WalkExRecuProc", xrtAVLTB_WalkExRecuProc);
	
	// 添加函数 - AVLTree
	tcc_add_symbol(s, "xrtAVLTreeCreate", xrtAVLTreeCreate);
	tcc_add_symbol(s, "xrtAVLTreeDestroy", xrtAVLTreeDestroy);
	tcc_add_symbol(s, "xrtAVLTreeInit", xrtAVLTreeInit);
	tcc_add_symbol(s, "xrtAVLTreeUnit", xrtAVLTreeUnit);
	tcc_add_symbol(s, "xrtAVLTreeInsert", xrtAVLTreeInsert);
	tcc_add_symbol(s, "xrtAVLTreeRemove", xrtAVLTreeRemove);
	tcc_add_symbol(s, "xrtAVLTreeSearch", xrtAVLTreeSearch);
	
	// 添加函数 - Memory Pool
	tcc_add_symbol(s, "xrtMemPoolCreate", xrtMemPoolCreate);
	tcc_add_symbol(s, "xrtMemPoolDestroy", xrtMemPoolDestroy);
	tcc_add_symbol(s, "xrtMemPoolInit", xrtMemPoolInit);
	tcc_add_symbol(s, "xrtMemPoolUnit", xrtMemPoolUnit);
	tcc_add_symbol(s, "xrtMemPoolAlloc", xrtMemPoolAlloc);
	tcc_add_symbol(s, "xrtMemPoolFree", xrtMemPoolFree);
	tcc_add_symbol(s, "xrtMemPoolGC", xrtMemPoolGC);
	
	// 添加函数 - Dict
	tcc_add_symbol(s, "xrtDictCreate", xrtDictCreate);
	tcc_add_symbol(s, "xrtDictDestroy", xrtDictDestroy);
	tcc_add_symbol(s, "xrtDictInit", xrtDictInit);
	tcc_add_symbol(s, "xrtDictUnit", xrtDictUnit);
	tcc_add_symbol(s, "xrtDictSet", xrtDictSet);
	tcc_add_symbol(s, "xrtDictSetPtr", xrtDictSetPtr);
	tcc_add_symbol(s, "xrtDictGet", xrtDictGet);
	tcc_add_symbol(s, "xrtDictGetPtr", xrtDictGetPtr);
	tcc_add_symbol(s, "xrtDictRemove", xrtDictRemove);
	tcc_add_symbol(s, "xrtDictRemovePtr", xrtDictRemovePtr);
	tcc_add_symbol(s, "xrtDictExists", xrtDictExists);
	tcc_add_symbol(s, "xrtDictCount", xrtDictCount);
	tcc_add_symbol(s, "xrtDictWalk", xrtDictWalk);
	
	// 添加函数 - List
	tcc_add_symbol(s, "xrtListCreate", xrtListCreate);
	tcc_add_symbol(s, "xrtListDestroy", xrtListDestroy);
	tcc_add_symbol(s, "xrtListInit", xrtListInit);
	tcc_add_symbol(s, "xrtListUnit", xrtListUnit);
	tcc_add_symbol(s, "xrtListSet", xrtListSet);
	tcc_add_symbol(s, "xrtListSetPtr", xrtListSetPtr);
	tcc_add_symbol(s, "xrtListGet", xrtListGet);
	tcc_add_symbol(s, "xrtListGetPtr", xrtListGetPtr);
	tcc_add_symbol(s, "xrtListRemove", xrtListRemove);
	tcc_add_symbol(s, "xrtListRemovePtr", xrtListRemovePtr);
	tcc_add_symbol(s, "xrtListExists", xrtListExists);
	tcc_add_symbol(s, "xrtListCount", xrtListCount);
	tcc_add_symbol(s, "xrtListWalk", xrtListWalk);
	
	// 添加函数 - Value
	tcc_add_symbol(s, "xvoAddRef", xvoAddRef);
	tcc_add_symbol(s, "xvoUnref", xvoUnref);
	tcc_add_symbol(s, "xvoCreateNull", xvoCreateNull);
	tcc_add_symbol(s, "xvoCreateBool", xvoCreateBool);
	tcc_add_symbol(s, "xvoCreateInt", xvoCreateInt);
	tcc_add_symbol(s, "xvoCreateFloat", xvoCreateFloat);
	tcc_add_symbol(s, "xvoCreateText", xvoCreateText);
	tcc_add_symbol(s, "xvoCreateTime", xvoCreateTime);
	tcc_add_symbol(s, "xvoCreateTimeSerial", xvoCreateTimeSerial);
	tcc_add_symbol(s, "xvoCreatePoint", xvoCreatePoint);
	tcc_add_symbol(s, "xvoCreateFunc", xvoCreateFunc);
	tcc_add_symbol(s, "xvoCreateArray", xvoCreateArray);
	tcc_add_symbol(s, "xvoCreateList", xvoCreateList);
	tcc_add_symbol(s, "xvoCreateColl", xvoCreateColl);
	tcc_add_symbol(s, "xvoCreateTable", xvoCreateTable);
	tcc_add_symbol(s, "xvoCreateClass", xvoCreateClass);
	tcc_add_symbol(s, "xvoCreateCustom", xvoCreateCustom);
	tcc_add_symbol(s, "xvoGetBool", xvoGetBool);
	tcc_add_symbol(s, "xvoGetInt", xvoGetInt);
	tcc_add_symbol(s, "xvoGetFloat", xvoGetFloat);
	tcc_add_symbol(s, "xvoGetText", xvoGetText);
	tcc_add_symbol(s, "xvoGetTime", xvoGetTime);
	tcc_add_symbol(s, "xvoGetPoint", xvoGetPoint);
	tcc_add_symbol(s, "xvoGetFunc", xvoGetFunc);
	tcc_add_symbol(s, "xvoGetArray", xvoGetArray);
	tcc_add_symbol(s, "xvoGetList", xvoGetList);
	tcc_add_symbol(s, "xvoGetColl", xvoGetColl);
	tcc_add_symbol(s, "xvoGetTable", xvoGetTable);
	tcc_add_symbol(s, "xvoGetClass", xvoGetClass);
	tcc_add_symbol(s, "xvoGetCustom", xvoGetCustom);
	tcc_add_symbol(s, "xvoArrayGetValue", xvoArrayGetValue);
	tcc_add_symbol(s, "xvoArrayAppendValue", xvoArrayAppendValue);
	tcc_add_symbol(s, "xvoArrayInsertValue", xvoArrayInsertValue);
	tcc_add_symbol(s, "xvoArraySetValue", xvoArraySetValue);
	tcc_add_symbol(s, "xvoArrayMerge", xvoArrayMerge);
	tcc_add_symbol(s, "xvoArraySwap", xvoArraySwap);
	tcc_add_symbol(s, "xvoArrayRemove", xvoArrayRemove);
	tcc_add_symbol(s, "xvoArrayItemCount", xvoArrayItemCount);
	tcc_add_symbol(s, "xvoArrayClear", xvoArrayClear);
	tcc_add_symbol(s, "xvoArrayAlloc", xvoArrayAlloc);
	tcc_add_symbol(s, "xvoArraySort", xvoArraySort);
	tcc_add_symbol(s, "xvoListGetValue", xvoListGetValue);
	tcc_add_symbol(s, "xvoListSetValue", xvoListSetValue);
	tcc_add_symbol(s, "xvoListMerge", xvoListMerge);
	tcc_add_symbol(s, "xvoListExists", xvoListExists);
	tcc_add_symbol(s, "xvoListRemove", xvoListRemove);
	tcc_add_symbol(s, "xvoListItemCount", xvoListItemCount);
	tcc_add_symbol(s, "xvoListClear", xvoListClear);
	tcc_add_symbol(s, "xvoListSetParent", xvoListSetParent);
	tcc_add_symbol(s, "xvoCollSetValue", xvoCollSetValue);
	tcc_add_symbol(s, "xvoCollDifference", xvoCollDifference);
	tcc_add_symbol(s, "xvoCollSymmetricDifference", xvoCollSymmetricDifference);
	tcc_add_symbol(s, "xvoCollIntersection", xvoCollIntersection);
	tcc_add_symbol(s, "xvoCollUnion", xvoCollUnion);
	tcc_add_symbol(s, "xvoCollMerge", xvoCollMerge);
	tcc_add_symbol(s, "xvoCollExists", xvoCollExists);
	tcc_add_symbol(s, "xvoCollRemove", xvoCollRemove);
	tcc_add_symbol(s, "xvoCollItemCount", xvoCollItemCount);
	tcc_add_symbol(s, "xvoCollClear", xvoCollClear);
	tcc_add_symbol(s, "xvoCollSetParent", xvoCollSetParent);
	tcc_add_symbol(s, "xvoTableGetValue", xvoTableGetValue);
	tcc_add_symbol(s, "xvoTableSetValue", xvoTableSetValue);
	tcc_add_symbol(s, "xvoTableMerge", xvoTableMerge);
	tcc_add_symbol(s, "xvoTableExists", xvoTableExists);
	tcc_add_symbol(s, "xvoTableRemove", xvoTableRemove);
	tcc_add_symbol(s, "xvoTableItemCount", xvoTableItemCount);
	tcc_add_symbol(s, "xvoTableClear", xvoTableClear);
	tcc_add_symbol(s, "xvoTableSetParent", xvoTableSetParent);
	tcc_add_symbol(s, "xvoIsNull", xvoIsNull);
	tcc_add_symbol(s, "xvoType", xvoType);
	tcc_add_symbol(s, "xvoGetSize", xvoGetSize);
	tcc_add_symbol(s, "xvoCopy", xvoCopy);
	tcc_add_symbol(s, "xvoDeepCopy", xvoDeepCopy);
	tcc_add_symbol(s, "xvoPrintValue", xvoPrintValue);
	
	// 添加函数 - JNUM
	tcc_add_symbol(s, "xrtI32ToStr", xrtI32ToStr);
	tcc_add_symbol(s, "xrtI64ToStr", xrtI64ToStr);
	tcc_add_symbol(s, "xrtU32ToStr", xrtU32ToStr);
	tcc_add_symbol(s, "xrtU64ToStr", xrtU64ToStr);
	tcc_add_symbol(s, "xrtNumToStr", xrtNumToStr);
	tcc_add_symbol(s, "xrtParseNum", xrtParseNum);
	tcc_add_symbol(s, "xrtStrToI32", xrtStrToI32);
	tcc_add_symbol(s, "xrtStrToI64", xrtStrToI64);
	tcc_add_symbol(s, "xrtStrToU32", xrtStrToU32);
	tcc_add_symbol(s, "xrtStrToU64", xrtStrToU64);
	tcc_add_symbol(s, "xrtStrToNum", xrtStrToNum);
	
	// 添加函数 - JSON
	tcc_add_symbol(s, "xrtJsonGetStringInfo", xrtJsonGetStringInfo);
	tcc_add_symbol(s, "xrtJsonParseSAX", xrtJsonParseSAX);
	tcc_add_symbol(s, "xrtJsonPrintStart", xrtJsonPrintStart);
	tcc_add_symbol(s, "xrtJsonPrintValue", xrtJsonPrintValue);
	tcc_add_symbol(s, "xrtJsonPrintFinish", xrtJsonPrintFinish);
	tcc_add_symbol(s, "xrtParseJSON", xrtParseJSON);
	tcc_add_symbol(s, "xrtParseJSON_File", xrtParseJSON_File);
	tcc_add_symbol(s, "xrtStringifyJSON", xrtStringifyJSON);
	tcc_add_symbol(s, "xrtStringifyJSON_File", xrtStringifyJSON_File);
	
	// 添加函数 - Template
	tcc_add_symbol(s, "xteCreateIdentList", xteCreateIdentList);
	tcc_add_symbol(s, "xteDestroyIdentList", xteDestroyIdentList);
	tcc_add_symbol(s, "xteAddIdentToList", xteAddIdentToList);
	tcc_add_symbol(s, "xteLexerFree", xteLexerFree);
	tcc_add_symbol(s, "xteLexer", xteLexer);
	tcc_add_symbol(s, "xteParseFromTokenList", xteParseFromTokenList);
	tcc_add_symbol(s, "xteParse", xteParse);
	tcc_add_symbol(s, "xteParseFree", xteParseFree);
	tcc_add_symbol(s, "xteMakeActions", xteMakeActions);
	tcc_add_symbol(s, "xteMake", xteMake);
	tcc_add_symbol(s, "xteResolvePath", xteResolvePath);
	tcc_add_symbol(s, "xteExprParse", xteExprParse);
	tcc_add_symbol(s, "xteExprFree", xteExprFree);
	tcc_add_symbol(s, "xteExprEval", xteExprEval);
	tcc_add_symbol(s, "xteExprEvalBool", xteExprEvalBool);
}


