extern int deliberately_missing_native_symbol(void);

int xl_sdk_feature_probe(void)
{
    return deliberately_missing_native_symbol();
}
