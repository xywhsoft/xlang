/* Generated fixture is preincluded; executable and callbacks stay resident. */
extern uint64_t xrtClock(void);
extern void xrtThreadYield(void);
int main(void)
{
    int ok = xlir_module_init();
    uint64_t start = xrtClock();
    if (!ok) {
        const xerror* error = xrtGetError();
        fprintf(stderr, "network source init failed: %s\n", error ? xrtErrorMessage(error) : "without diagnostic");
    }
    while (!xlir_continuation_quiesce()) {
        if (xrtClock() - start > 5000000) { fprintf(stderr, "network source did not retire within 5 seconds\n"); return 2; }
        xrtThreadYield();
    }
    xlir_module_fini();
    if (xrtGetError()) { fprintf(stderr, "network source finalization failed: %s\n", xrtErrorMessage(xrtGetError())); return 3; }
    return ok ? 0 : 1;
}
