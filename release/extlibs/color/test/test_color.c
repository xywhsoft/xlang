#define XRT_MODULE_MEMORY_DEBUG
#include "../include/color_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void core(void) {
    char a[XCOLOR_NAME_SIZE], b[XCOLOR_NAME_SIZE];
    const char *bad[] = {NULL, "", "#", "##abc", "#12", "#12345", "#1234567",
        "#123456789", "#GGG", "red", " #fff", "#fff ", "你好"};
    for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        assert(!xcolor_valid(bad[i]));
        assert(xcolor_red(bad[i])==-1 && xcolor_green(bad[i])==-1);
        assert(xcolor_blue(bad[i])==-1 && xcolor_alpha(bad[i])==-1);
        assert(xcolor_hue(bad[i])==-1 && xcolor_saturation(bad[i])==-1);
        assert(xcolor_value(bad[i])==-1 && xcolor_lightness(bad[i])==-1);
        assert(xcolor_luminance(bad[i])==-1 && xcolor_contrast(bad[i],"#fff")==-1);
        assert(!xcolor_is_dark(bad[i]) && !xcolor_is_light(bad[i]));
        assert(!xcolor_normalize(bad[i],a,sizeof(a)) && a[0]==0);
        assert(!xcolor_to_name(bad[i],a,sizeof(a)) && a[0]==0);
        assert(!xcolor_mix("#000",bad[i],0.5,a,sizeof(a)) && a[0]==0);
        assert(xrtGetError()==NULL);
    }
    assert(xcolor_valid("#AbC") && xcolor_valid("aBcD"));
    assert(xcolor_red("#abc")==170 && xcolor_green("#abc")==187);
    assert(xcolor_blue("#abcd")==204 && xcolor_alpha("#abcd")==221);
    assert(xcolor_hex(0,128,255,a,sizeof(a)) && !strcmp(a,"#0080FF"));
    assert(xcolor_hex_a(0,128,255,64,a,sizeof(a)) && !strcmp(a,"#0080FF40"));
    assert(!xcolor_hex(INT64_MAX,0,0,a,sizeof(a)) && a[0]==0);
    assert(!xcolor_hex_a(0,0,0,INT64_MIN,a,sizeof(a)) && a[0]==0);
    assert(xcolor_normalize("#aBcD",a,sizeof(a)) && !strcmp(a,"#AABBCC"));
    assert(xcolor_hue("#00f")==240 && xcolor_saturation("#888")==0);
    assert(xcolor_lightness("#ff0000")==0.5 && xcolor_value("#000")==0);
    assert(xcolor_from_hsv(0,-1,0.5,a,sizeof(a)) && !strcmp(a,"#808080"));
    assert(xcolor_from_hsv(120,2,2,a,sizeof(a)) && !strcmp(a,"#00FF00"));
    assert(xcolor_from_hsv(-120,1,1,a,sizeof(a)) && !strcmp(a,"#0000FF"));
    assert(xcolor_from_hsl(720,1,0.5,a,sizeof(a)) && !strcmp(a,"#FF0000"));
    assert(xcolor_from_hsl(0,-1,2,a,sizeof(a)) && !strcmp(a,"#FFFFFF"));
    assert(xcolor_mix("#0000", "#ffff",-1,a,sizeof(a)) && !strcmp(a,"#000000"));
    assert(xcolor_mix("#000", "#fff",0.5,a,sizeof(a)) && !strcmp(a,"#808080"));
    assert(xcolor_mix("#000", "#fff",2,a,sizeof(a)) && !strcmp(a,"#FFFFFF"));
    assert(xcolor_contrast("#000", "#fff")==21);
    assert(xcolor_luminance("#fff")==1 && xcolor_luminance("#000")==0);
    assert(xcolor_is_dark("#000") && xcolor_is_light("#fff"));
    assert(xcolor_from_name("RebeccaPurple",a,sizeof(a)) && !strcmp(a,"#663399"));
    assert(xcolor_from_name("lightgoldenrodyellow",a,sizeof(a)) && !strcmp(a,"#FAFAD2"));
    assert(!xcolor_from_name("transparent",a,sizeof(a)) && !xcolor_from_name(NULL,a,sizeof(a)));
    assert(!xcolor_from_name("rebeccapurple________________________",a,sizeof(a)));
    assert(xcolor_to_name("#00ffff",a,sizeof(a)) && !strcmp(a,"aqua"));
    assert(xcolor_to_name("#ff00ff",a,sizeof(a)) && !strcmp(a,"fuchsia"));
    assert(xcolor_to_name("#808080",a,sizeof(a)) && !strcmp(a,"gray"));
    assert(!xcolor_to_name("#0000",a,sizeof(a)) && a[0]==0);
    assert(!xcolor_to_name("#010203",a,sizeof(a)) && a[0]==0);
    const double special[]={NAN,INFINITY,-INFINITY};
    for(size_t i=0;i<3;++i) {
        double n=special[i];
        assert(!xcolor_from_hsv(n,1,1,a,sizeof(a)) && a[0]==0);
        assert(!xcolor_from_hsv(0,n,1,a,sizeof(a)) && a[0]==0);
        assert(!xcolor_from_hsv(0,1,n,a,sizeof(a)) && a[0]==0);
        assert(!xcolor_from_hsl(n,1,0.5,a,sizeof(a)) && a[0]==0);
        assert(!xcolor_from_hsl(0,n,0.5,a,sizeof(a)) && a[0]==0);
        assert(!xcolor_from_hsl(0,1,n,a,sizeof(a)) && a[0]==0);
        assert(!xcolor_mix("#000","#fff",n,a,sizeof(a)) && a[0]==0);
        assert(xrtGetError()==NULL);
    }
    assert(xcolor_from_hsv(DBL_MAX,DBL_MAX,DBL_MAX,a,sizeof(a)) && xcolor_valid(a));
    assert(xcolor_from_hsl(-DBL_MAX,-DBL_MAX,-DBL_MAX,a,sizeof(a)) && !strcmp(a,"#000000"));
    strcpy(a,"#f00");
    assert(xcolor_normalize(a,a,sizeof(a)) && !strcmp(a,"#FF0000"));
    strcpy(a,"RebeccaPurple");
    assert(xcolor_from_name(a,a,sizeof(a)) && !strcmp(a,"#663399"));
    assert(xcolor_to_name(a,a,sizeof(a)) && !strcmp(a,"rebeccapurple"));
    strcpy(a,"#000"); strcpy(b,"#fff");
    assert(xcolor_mix(a,b,0.5,a,sizeof(a)) && !strcmp(a,"#808080"));

    /* 每个通道遍历全部字节值，并交叉检查 alpha 去除与 RGB 构造。 */
    for (int i = 0; i < 1024; ++i) {
        int64_t r = i & 255;
        int64_t g = (i * 73 + (i >> 8) * 11 + 19) & 255;
        int64_t blue = (i * 151 + (i >> 8) * 7 + 67) & 255;
        int64_t alpha = (i * 199 + (i >> 8) * 13 + 29) & 255;
        assert(xcolor_hex_a(r, g, blue, alpha, a, sizeof(a)));
        assert(xcolor_valid(a) && xcolor_red(a) == r && xcolor_green(a) == g);
        assert(xcolor_blue(a) == blue && xcolor_alpha(a) == alpha);
        assert(xcolor_normalize(a, b, sizeof(b)));
        assert(xcolor_hex(r, g, blue, a, sizeof(a)) && !strcmp(a, b));
    }
}

static void outputs(void) {
    char storage[32];
    for(size_t n=0;n<=10;++n) {
        memset(storage,0x5a,sizeof(storage));
        bool ok=xcolor_hex_a(1,2,3,4,storage+1,n);
        assert(ok==(n>=10));
        assert(storage[0]==0x5a && storage[n+1]==0x5a);
        if(n==0) assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
        else if(n<10) assert(storage[1]==0 && xrtErrorKind(xrtGetError())==XERR_RANGE);
        else assert(!strcmp(storage+1,"#01020304") && xrtGetError()==NULL);
    }
    assert(!xcolor_hex(0,0,0,NULL,8) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    assert(xcolor_valid("#fff") && xrtGetError()==NULL);
    assert(!xcolor_valid("bad!") && xrtGetError()==NULL);
}

static void* text_result(int index) {
    switch(index) {
    case 0: return xcolor_hex_text(1,2,3);
    case 1: return xcolor_hex_a_text(1,2,3,4);
    case 2: return xcolor_normalize_text("#abc");
    case 3: return xcolor_from_hsv_text(0,1,1);
    case 4: return xcolor_from_hsl_text(120,1,0.5);
    case 5: return xcolor_mix_text("#000","#fff",0.5);
    case 6: return xcolor_from_name_text("red");
    default: return xcolor_to_name_text("#fff");
    }
}

int main(void) {
    xmemdebugsnapshot before,after;
    xrtMemDebugSnapshot(&before);
    core(); outputs();
    assert(xrtMemDebugFailAfter(0));
    core();
    assert(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    const char *expected[]={"#010203","#01020304","#AABBCC","#FF0000","#00FF00","#808080","#FF0000","white"};
    for(int i=0;i<8;++i) {
        assert(xrtMemDebugFailAfter(0));
        assert(text_result(i)==NULL && xrtMemDebugFailTriggered());
        assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);
        xrtMemDebugFailClear();
        char *keep=text_result(i);
        assert(keep && !strcmp(keep,expected[i]));
        for(int j=0;j<32;++j) xrtFree(text_result(j%8));
        assert(!strcmp(keep,expected[i]));
        xrtFree(keep);
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.OverflowCount==after.OverflowCount && before.UnderflowCount==after.UnderflowCount);
    puts("color C: 21 APIs, buffer bounds/aliasing, finite math, zero-allocation core and 8 owned-result failures PASS");
    return 0;
}
