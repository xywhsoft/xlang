typedef int (*IntCallback)(int value);

int c_apply_i32(IntCallback callback, int value)
{
    return callback != 0 ? callback(value) : -1;
}

static int triple(int value)
{
    return value * 3;
}

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }
    if ( xl_run_xlang_callback() != 42 ) {
        result = 2;
    } else if ( xl_apply_c_callback(triple, 14) != 42 ) {
        result = 3;
    }
    xlUnit();
    return result;
}
