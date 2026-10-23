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
