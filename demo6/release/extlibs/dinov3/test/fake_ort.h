#ifndef DINOV3_TEST_FAKE_ORT_H
#define DINOV3_TEST_FAKE_ORT_H

typedef struct DinoFakeResult {
    const char* shape;
    int count;
    const float* data;
} DinoFakeResult;

#endif
