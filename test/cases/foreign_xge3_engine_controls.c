#include "foreign_xge3_engine_controls.h"
#include "foreign_xge3_state_oom.c"

static int quitCalls;
static int errorCalls;

static int vetoQuit(void* user)
{
    ++*(int*)user;
    return 0;
}

static void receiveError(const xge_error_info_t* info, void* user)
{
    assert(info->iCode == XGE_ERROR_INVALID_ARGUMENT);
    ++*(int*)user;
}

int fixtureInstallLegacyEngineCallbacks(void)
{
    xgeSetQuitRequestCallback(vetoQuit, &quitCalls);
    return xgeSetErrorCallback(receiveError, &errorCalls);
}

int fixtureLegacyQuitCalls(void) { return quitCalls; }
int fixtureLegacyErrorCalls(void) { return errorCalls; }
