#ifndef FOREIGN_OWNED_ADMISSION_CALLS_H
#define FOREIGN_OWNED_ADMISSION_CALLS_H
#include "foreign_output_snapshots.h"
const xerror* admissionCountGet(void);
xerror* admissionCountTake(void);
void admissionCountRelease(xvalue*);
void admissionCountSet(xerror*);
void admissionCountReset(void);
void admissionCountReport(void);
#endif
