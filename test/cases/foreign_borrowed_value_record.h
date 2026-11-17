#ifndef FOREIGN_BORROWED_VALUE_RECORD_H
#define FOREIGN_BORROWED_VALUE_RECORD_H
#include <stdint.h>
#include <__xl_runtime/xrt_decl.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct BorrowedValueRecord {uint32_t iSize;const char* name;xvalue* data;const char* tail;} BorrowedValueRecord;
typedef struct BorrowedValuePair {xvalue* first;xvalue* second;} BorrowedValuePair;
typedef struct BorrowedValueReceiver {int mode;} BorrowedValueReceiver;
void borrowedRecordSet(xvalue*);
void borrowedRecordClear(void);
int borrowedRecordRead(int,BorrowedValueRecord*);
int borrowedRecordReadPair(BorrowedValuePair*);
BorrowedValueReceiver* borrowedRecordReceiver(void);
int borrowedRecordReadBound(BorrowedValueReceiver*,BorrowedValueRecord*);
int borrowedRecordCalls(void);
void borrowedRecordBudget(int);
void borrowedRecordMark(void);
int borrowedRecordBalanced(void);
#ifdef __cplusplus
}
#endif
#endif
