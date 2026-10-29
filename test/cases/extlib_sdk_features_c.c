#include "../fixtures/sdk_feature_probe/sdk_features_native.c"
#include <stdio.h>

int main(void)
{
    if (xl_sdk_feature_probe() != 0) return 1;
    puts("SDK selected features ok");
    return 0;
}
