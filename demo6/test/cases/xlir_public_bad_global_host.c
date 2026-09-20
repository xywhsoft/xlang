/* Deliberately disagrees with the generated int64_t extern variable ABI. */
int host_counter = 40;
void expect_i64(long long actual, long long expected)
{
    (void)actual;
    (void)expected;
}
