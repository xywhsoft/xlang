#ifndef XFEAT_FAKE_ORT_H
#define XFEAT_FAKE_ORT_H

typedef struct FakeOrtResult {
    const char* shape;
    int count;
    const float* data;
} FakeOrtResult;

#endif
