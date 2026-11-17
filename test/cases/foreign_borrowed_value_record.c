#include "test/cases/foreign_borrowed_value_record.h"
#include <assert.h>
#include <stdio.h>
static xvalue* held;
static int calls;
void borrowedRecordSet(xvalue* value){xvalue* next=xrtValueRetain(value);assert(next);xvalue* old=held;held=next;xrtValueRelease(old);}
void borrowedRecordClear(void){xvalue* old=held;held=NULL;xrtValueRelease(old);}
int borrowedRecordRead(int mode,BorrowedValueRecord* output)
{++calls;assert(output && output->iSize==sizeof(*output));output->name="record";output->tail="tail";output->data=held;
 if(mode==1){output->data=(xvalue*)(uintptr_t)1;return -16;}
 if(mode==2)output->data=NULL;
 if(mode==3){output->name=NULL;output->tail="";}
 return 0;}
int borrowedRecordCalls(void){return calls;}

int borrowedRecordReadPair(BorrowedValuePair* output)
{
    ++calls;
    output->first=held;
    output->second=held;
    return 0;
}

BorrowedValueReceiver* borrowedRecordReceiver(void)
{
    static BorrowedValueReceiver receiver={0};
    return &receiver;
}

int borrowedRecordReadBound(BorrowedValueReceiver* receiver,BorrowedValueRecord* output)
{
    return borrowedRecordRead(receiver->mode,output);
}

static xmemdebugsnapshot baseline;
void borrowedRecordBudget(int allowance)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));if(allowance<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter((size_t)allowance));}
void borrowedRecordMark(void){xrtMemDebugSnapshot(&baseline);}
int borrowedRecordBalanced(void){xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);int ok=after.LiveCount==baseline.LiveCount && after.LiveBytes==baseline.LiveBytes && after.InvalidFreeCount==baseline.InvalidFreeCount && after.DoubleFreeCount==baseline.DoubleFreeCount;if(!ok)fprintf(stderr,"borrowed record heap: %llu/%llu -> %llu/%llu\n",(unsigned long long)baseline.LiveCount,(unsigned long long)baseline.LiveBytes,(unsigned long long)after.LiveCount,(unsigned long long)after.LiveBytes);return ok;}
