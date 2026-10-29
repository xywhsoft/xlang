#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(xrtMemDebugReset());
    unsigned char marked[] = {0xEF,0xBB,0xBF,0xFF,0};
    xvalue* value = xrtValueBytes((xbytesview){marked,sizeof(marked)}); assert(value);
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for (int i=0;i<10000;++i) {
        XL6_ProductI64x3V1 guess = xl6EncodingGuessBytesV1(value,-1);
        assert(guess.e0 == 1 && guess.e1 == 3 && guess.e2 == 100);
        guess = xl6EncodingGuessTextV1((xstrview){"A\0B",3},-1);
        assert(guess.e0 == 1 && guess.e1 == 0 && guess.e2 == 50);
        guess = xl6EncodingGuessBytesV1(value,0);
        assert(!guess.e0 && !guess.e1 && !guess.e2);
    }
    xrtMemDebugSnapshot(&after); assert(before.AllocCount == after.AllocCount && !xrtGetError());
    /* Ordinary guesses succeed while every allocator request is denied. */
    assert(xrtMemDebugFailAfter(0));
    XL6_ProductI64x3V1 guess = xl6EncodingGuessBytesV1(value,1000);
    assert(guess.e0 == 1 && guess.e1 == 3 && guess.e2 == 100 && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    guess=xl6EncodingGuessBytesV1(value,-2);
    assert(!guess.e0 && !guess.e1 && !guess.e2 && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    guess=xl6EncodingGuessBytesV1(NULL,-1);
    assert(!guess.e0 && !guess.e1 && !guess.e2 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    guess=xl6EncodingGuessTextV1((xstrview){NULL,1},-1);
    assert(!guess.e0 && !guess.e1 && !guess.e2 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    guess=xl6EncodingGuessTextV1((xstrview){"x",SIZE_MAX},-1);
    assert(!guess.e0 && !guess.e1 && !guess.e2 && xrtErrorKind(xrtGetError()) == XERR_RANGE); xrtClearError();
    xrtValueRelease(value); assert(xrtMemDebugReset());
    puts("encoding guess: trivial exact product, NUL/caps/BOM-not-validation, 10000 queries no allocation and allocator-denial passed");
    return 0;
}
