#include <assert.h>
#include <string.h>

/* Only the explicit C-string assertion used by the legacy scalar fixture.
 * Language-value copying, nullable projection and inference stay in xlang. */
void expect_text(const char* actual, const char* expected)
{
    assert(actual && expected && !strcmp(actual,expected));
}
