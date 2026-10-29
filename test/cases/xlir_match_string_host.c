/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_match_string_generated.c"

int main(void)
{
    return xlir_module_init() == 1 &&
           xlir_test_match_string((xlir_string){"yes", 3}) == 10 &&
           xlir_test_match_string((xlir_string){"no", 2}) == 20 &&
           xlir_test_match_string((xlir_string){"a\0b", 3}) == 40 &&
           xlir_test_match_string((xlir_string){"other", 5}) == 30 ? 0 : 1;
}
