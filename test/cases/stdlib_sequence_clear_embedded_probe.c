/* Test-only callback producer; compile against the product's own VFS SDK.
 * No container implementation or owning/error adapter is substituted. */
#define XL6_CLEAR_EMBEDDED_SDK 1
#include "../support/sequence_clear_probe.c"
