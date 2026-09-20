int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    xvalue* value;
    xvalue* copied;
    xvalue* output;
    int64_t number;
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    value = xl6_class_make(41);
    if ( xlValueIsNull(value) ) {
        xlUnit();
        return 2;
    }
    copied = xlValueClone(value, false);
    if ( copied == NULL || copied == value || xl6_class_read(copied) != 42 ) {
        xlValueRelease(copied);
        xlValueRelease(value);
        xlUnit();
        return 5;
    }
    xlValueRelease(copied);
    copied = xlValueRetain(value);
    if ( xl6_class_read(copied) != 42 ) {
        xlValueRelease(copied);
        xlValueRelease(value);
        xlUnit();
        return 6;
    }
    xlValueRelease(copied);
    result = (int)xl6_class_read(value);
    number = 1;
    xl6_mode_increment(&number);
    output = NULL;
    xl6_mode_make(&output);
    if ( number != 2 || output == NULL || xl6_class_read(output) != 10 ) {
        xlValueRelease(value);
        if ( output != NULL ) {
            xlValueRelease(output);
        }
        xlUnit();
        return 3;
    }
    xlValueRelease(value);
    xlValueRelease(output);
    xlUnit();
    return result == 42 ? 0 : 4;
}
