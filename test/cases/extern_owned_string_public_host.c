#include <stdbool.h>
#include <stddef.h>
extern char* owned_text(void);
char* owned_public_native(bool present) { return present ? owned_text() : NULL; }
