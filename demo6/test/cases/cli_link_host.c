void exit(int code);

long long cli_link_value(void)
{
    return 42;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(1);
    }
}
