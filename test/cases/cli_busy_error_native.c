#include "cli_busy_error.h"
struct CliBusy {int marker;};
static CliBusy retained={1};
int cliBusyCreate(CliBusy** output){*output=&retained;return 0;}
/* Static test payload remains valid. Real production owners retain their
 * exact native/code obligations; process failure must not pretend to free it. */
void cliBusyRollback(CliBusy* raw){(void)raw;}
