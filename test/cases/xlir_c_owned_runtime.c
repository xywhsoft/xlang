#include "src/runtime/xl6_c_owned.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned released;
static XL6_COwnedV1* observedOwner;
static void release(void* data)
{
    if (observedOwner) {
        assert(!observedOwner->Data && !observedOwner->Size && !observedOwner->Owner);
    }
    ++released;
    free(data);
}
static void* allocation(const void* bytes, size_t size)
{
    void* result = malloc(size ? size : 1);
    assert(result);
    if (size) memcpy(result, bytes, size);
    return result;
}

static int32 shareOwner(void* raw)
{
    const XL6_COwnedV1* source=raw;
    for(unsigned i=0;i<10000;++i) {
        XL6_COwnedV1 copy=xl6COwnedCopyV1(*source);
        assert(copy.Data==source->Data && copy.Size==source->Size);
        xl6COwnedDropV1(&copy);
    }
    return 0;
}

int main(void)
{
    const char text[] = "A\xe4\xbd\xa0\xe5\xa5\xbd";
    const unsigned char binary[] = {0, 255, 17, 0, 42};
    XL6_COwnedV1 c = xl6CStringTakeV1(allocation(text, sizeof(text)), release);
    assert(c.Size == sizeof(text)-1 && released == 0);
    xstrview copied = xl6CStringCopyV1(c);
    assert(copied.Data && copied.Data != c.Data && copied.Size == c.Size);
    assert(!memcmp(copied.Data, text, sizeof(text)));
    xvalue* boxed = xl6CStringBoxV1(c);
    assert(boxed && xrtValueType(boxed) == XVALUE_STRING);
    xstrview boxedText = {0};
    assert(xrtValueGetString(boxed, &boxedText));
    assert(boxedText.Data != c.Data && boxedText.Size == c.Size);
    assert(!memcmp(boxedText.Data, text, sizeof(text)-1));
    XL6_COwnedV1 shared=xl6COwnedCopyV1(c);
    assert(shared.Data==c.Data && shared.Owner==c.Owner);
    observedOwner = &c;
    xl6COwnedDropV1(&c);
    xl6COwnedDropV1(&c);
    observedOwner = NULL;
    assert(released==0 && xl6CStringViewV1(shared).Data);
    xl6COwnedDropV1(&shared);
    assert(released == 1 && !memcmp(copied.Data, text, sizeof(text)));
    xrtFree((void*)copied.Data);
    xrtValueRelease(boxed);

    XL6_COwnedV1 b = xl6CBytesTakeV1(allocation(binary, sizeof(binary)), sizeof(binary), release);
    xvalue* bytes = xl6CBytesCopyV1(b);
    assert(bytes && xrtValueType(bytes) == XVALUE_BYTES);
    xbytesview bytesView = {0};
    assert(xrtValueGetBytes(bytes, &bytesView));
    assert(bytesView.Data != b.Data && bytesView.Size == sizeof(binary));
    assert(!memcmp(bytesView.Data, binary, sizeof(binary)));
    xl6COwnedDropV1(&b);
    assert(released == 2 && !memcmp(bytesView.Data, binary, sizeof(binary)));
    xrtValueRelease(bytes);

    XL6_COwnedV1 empty = xl6CBytesTakeV1(allocation(NULL, 0), 0, release);
    xl6COwnedDropV1(&empty);
    assert(released == 3);

    XL6_COwnedV1 none = {NULL, SIZE_MAX, NULL};
    assert(xl6COwnedIsNoneV1(none) && !xl6COwnedIsNoneV1(empty));
    assert(!xl6CStringNullableViewV1(none).Data);
    assert(xl6CStringNullableViewV1(empty).Data && !xl6CStringNullableViewV1(empty).Size);
    assert(!xl6CStringCopyV1(none).Data);
    boxed = xl6CStringBoxV1(none);
    bytes = xl6CBytesCopyV1(none);
    assert(boxed && bytes && !xrtValueType(boxed) && !xrtValueType(bytes));
    xrtValueRelease(boxed);
    xrtValueRelease(bytes);
    shared = xl6COwnedCopyV1(none);
    assert(xl6COwnedIsNoneV1(shared));
    xl6COwnedDropV1(&shared);
    assert(xl6COwnedIsNoneV1(xl6CStringFromNullableStringV1((xstrview){0})));
    assert(xl6COwnedIsNoneV1(xl6CBytesFromNullableBytesV1(xrtValueRetain(xrtValueNull()))));
    assert(released == 3);
    empty = xl6CStringTakeV1(NULL, release);
    copied = xl6CStringCopyV1(empty);
    assert(copied.Data && !copied.Size && !*copied.Data);
    xrtFree((void*)copied.Data);
    bytes = xl6CBytesCopyV1(empty);
    assert(bytes && xrtValueType(bytes) == XVALUE_BYTES);
    assert(xrtValueGetBytes(bytes, &bytesView) && !bytesView.Size);
    xrtValueRelease(bytes);
    xl6COwnedDropV1(&empty);
    assert(released == 3);

    empty = xl6CBytesTakeV1(allocation(NULL, 0), -1, release);
    assert(!empty.Data && released == 4 && xrtGetError());
    xrtClearError();
    empty = xl6CBytesTakeV1(NULL, 1, release);
    assert(!empty.Data && released == 4 && xrtGetError());
    xrtClearError();
    assert(xrtMemDebugEnable(true));
    c=xl6CStringTakeV1(allocation(text,sizeof(text)),release);
    assert(xrtMemDebugFailAfter(0));
    copied=xl6CStringCopyV1(c);
    assert(!copied.Data && xrtMemDebugFailTriggered() && c.Data && released==4);
    xrtMemDebugFailClear();xrtClearError();
    copied=xl6CStringCopyV1(c);
    assert(copied.Data && copied.Data!=c.Data);
    xrtFree((void*)copied.Data);xl6COwnedDropV1(&c);
    b=xl6CBytesTakeV1(allocation(binary,sizeof(binary)),sizeof(binary),release);
    assert(xrtMemDebugFailAfter(0));
    bytes=xl6CBytesCopyV1(b);
    assert(!bytes && xrtMemDebugFailTriggered() && b.Data && released==5);
    xrtMemDebugFailClear();xrtClearError();xl6COwnedDropV1(&b);
    assert(released==6);
    assert(xrtMemDebugFailAfter(0));
    c=xl6CStringTakeV1(allocation(text,sizeof(text)),release);
    assert(!c.Data && !c.Owner && released==7 && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();xrtClearError();
    c=xl6CStringTakeV1(allocation(text,sizeof(text)),release);
    xthread* threads[4];
    for(size_t i=0;i<4;++i){threads[i]=xrtThreadCreate(shareOwner,&c,0);assert(threads[i]);}
    for(size_t i=0;i<4;++i){assert(xrtThreadWait(threads[i])==XWAIT_OK);xrtThreadDestroy(threads[i]);}
    assert(released==7 && xrtAtomic64Load(&c.Owner->References,XMEMORY_ACQUIRE)==1);
    xl6COwnedDropV1(&c);assert(released==8);
    const char embedded[]={'a',0,'b',0};
    c=xl6CBytesTakeV1(allocation(embedded,sizeof(embedded)),0,release);
    XL6_COwnedV1 sized={0};
    assert(xl6CStringSizeTakeV1(c,3,&sized) && sized.Size==3 && sized.Data==c.Data);
    copied=xl6CStringCopyV1(sized);
    assert(copied.Data && copied.Size==3 && !memcmp(copied.Data,embedded,3));
    xrtFree((void*)copied.Data);xl6COwnedDropV1(&sized);assert(released==9);
    c=xl6CBytesTakeV1(allocation(text,sizeof(text)),0,release);
    assert(!xl6CStringSizeTakeV1(c,1,&sized) && !sized.Data && released==10 && xrtGetError());
    xrtClearError();
    assert(xl6CStringSizeTakeV1((XL6_COwnedV1){0},0,&sized) && !sized.Data && !sized.Size);
    puts("C owned runtime: independent copies, length-aware text, binary NUL, empty, release-once and failed adoption passed");
    return 0;
}
