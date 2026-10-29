int main(int argc, char** argv)
{
    XL_InitOptions options = xlInitOptions(argc, argv);
    int64_t first = 0;
    int64_t second = 0;
    int code = 0;

    if ( !xlInit(&options) ) {
        return 10;
    }

    if ( xl6_feature_add_i64(20, 22) != 42 ) {
        code = 1;
    }
    xl6_feature_pair_i64(41, &first, &second);
    if ( code == 0 && (first != 41 || second != 42) ) {
        code = 2;
    }
    if ( code == 0 && xl6_feature_counter != 1 ) {
        code = 3;
    }
    xl6_feature_counter = 36;
    if ( code == 0 && xl6_feature_get_total() != 42 ) {
        code = 4;
    }

    xlUnit();
    return code;
}
