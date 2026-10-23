static bool __xrtProcessStop(
	xprocess* pProcess,
	xprocessstop Stop,
	bool (*pProc)(xprocess*)
)
{
	bool bOk;

	if ( pProcess == NULL ) {
		__xrtProcessErrorSet(
			XERR_ARGUMENT,
			XPROCESS_ERROR_ARGUMENT,
			"stop",
			"process is null",
			0
		);
		return false;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->State == XPROCESS_EXITED ) {
		(void)xrtMutexUnlock(&pProcess->Lock);
#if defined(_WIN32) || defined(_WIN64)
		/* The owned Job still identifies descendants after the root exits.
		   Preserve the terminal root status while cleaning that Job. */
		if ( Stop == XPROCESS_STOP_KILL_TREE ) return pProc(pProcess);
#endif
		return true;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	bOk = pProc(pProcess);
	if ( !bOk ) {
		return false;
	}
	(void)xrtMutexLock(&pProcess->Lock);
	if ( pProcess->RequestedStop < Stop ) {
		pProcess->RequestedStop = Stop;
	}
	(void)xrtMutexUnlock(&pProcess->Lock);
	return true;
}
