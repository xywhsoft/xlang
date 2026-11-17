#ifndef FOREIGN_FIXED_ARRAY_ZERO_INIT_H
#define FOREIGN_FIXED_ARRAY_ZERO_INIT_H
typedef struct fixed_zero_point_t {float x,y,z;} fixed_zero_point_t;
typedef struct fixed_zero_record_t {
    unsigned char bytes[7];
    unsigned short tags[5];
    fixed_zero_point_t points[3];
} fixed_zero_record_t;
#ifdef __cplusplus
extern "C" {
#endif
void fixtureFixedZeroInts(const int* values);
void fixtureFixedZeroPoints(const fixed_zero_point_t* values);
void fixtureFixedZeroRecord(const fixed_zero_record_t* value);
void fixtureFixedZeroHeapBegin(void);
void fixtureFixedZeroHeapEnd(void);
#ifdef __cplusplus
}
#endif
#endif
