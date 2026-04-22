typedef int (*ProbeCallback)(int value, LPCSTR text);
typedef void (CALLBACK *ProbeVoidCallback)(void);

typedef struct ProbeCallbackTable {
	ProbeCallback callback;
	int (*raw)(int value, LPCSTR text);
	void (CALLBACK *done)(void);
} ProbeCallbackTable;

int pkg_clock_artifact_callback_accept(ProbeCallback cb);
ProbeCallback pkg_clock_artifact_callback_identity(ProbeCallback cb);
void pkg_clock_artifact_callback_void(ProbeVoidCallback cb);
