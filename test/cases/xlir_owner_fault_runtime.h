#ifndef XLIR_OWNER_FAULT_RUNTIME_H
#define XLIR_OWNER_FAULT_RUNTIME_H
static xmemdebugsnapshot ownerFaultArmed;
void continuation_owner_fault_arm(int fault)
{
    xrtMemDebugSnapshot(&ownerFaultArmed);
    if (fault >= 0) assert(xrtMemDebugFailAfter((uint64)fault));
}
unsigned continuation_owner_fault_end(int fault)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    if (fault >= 0) { assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); }
    return (unsigned)(after.AllocCount - ownerFaultArmed.AllocCount);
}
static void continuation_owner_fault_balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (before->LiveCount != after.LiveCount || before->LiveBytes != after.LiveBytes)
        fprintf(stderr, "owner balance before=%llu/%llu after=%llu/%llu allocated=%llu freed=%llu\n",
            (unsigned long long)before->LiveCount, (unsigned long long)before->LiveBytes,
            (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes,
            (unsigned long long)(after.AllocCount - before->AllocCount),
            (unsigned long long)(after.FreeCount - before->FreeCount));
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount && before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount && !xrtGetError());
}
#endif
