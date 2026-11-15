/* Instrument emitted calls only; these wrappers are never product adapters. */
#include "foreign_owned_admission_calls.h"
#include "foreign_output_snapshots.c"
static unsigned long long admissionCalls[5];
const xerror* admissionCountGet(void){++admissionCalls[0];return xrtGetError();}
xerror* admissionCountTake(void){++admissionCalls[1];return xrtTakeError();}
void admissionCountRelease(xvalue* v){++admissionCalls[2];if(!v)++admissionCalls[4];xrtValueRelease(v);}
void admissionCountSet(xerror* e){++admissionCalls[3];xrtSetErrorTake(e);}
void admissionCountReset(void){memset(admissionCalls,0,sizeof(admissionCalls));}
void admissionCountReport(void)
{printf("Admission generated calls: get=%llu take=%llu release=%llu set=%llu nullRelease=%llu\n",admissionCalls[0],admissionCalls[1],admissionCalls[2],admissionCalls[3],admissionCalls[4]);}
