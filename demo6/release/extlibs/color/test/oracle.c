/* 独立参照驱动：逐行固定测试命令，不使用待测库解析期望数据。 */
#include "../include/color.h"
#include <stdio.h>

int main(void) {
    char line[256], output[XCOLOR_NAME_SIZE], input[64], other[64];
    double a,b,c;
    while(fgets(line,sizeof(line),stdin)) {
        if(line[0]=='r' && sscanf(line+1,"%63s",input)==1) {
            printf("%.17g %.17g %.17g %.17g %.17g\n",xcolor_hue(input),
                xcolor_saturation(input),xcolor_value(input),xcolor_lightness(input),xcolor_luminance(input));
        } else if((line[0]=='v' || line[0]=='l') && sscanf(line+1,"%lf %lf %lf",&a,&b,&c)==3) {
            bool ok=line[0]=='v' ? xcolor_from_hsv(a,b,c,output,sizeof(output)) : xcolor_from_hsl(a,b,c,output,sizeof(output));
            if(!ok) return 2;
            puts(output);
        } else if((line[0]=='n' || line[0]=='h') && sscanf(line+1,"%63s",input)==1) {
            bool ok=line[0]=='n' ? xcolor_from_name(input,output,sizeof(output)) : xcolor_to_name(input,output,sizeof(output));
            if(!ok) return 5;
            puts(output);
        } else if(line[0]=='c' && sscanf(line+1,"%63s %63s",input,other)==2) {
            printf("%.17g\n",xcolor_contrast(input,other));
        } else if(line[0]=='m' && sscanf(line+1,"%63s %63s %lf",input,other,&a)==3) {
            if(!xcolor_mix(input,other,a,output,sizeof(output))) return 6;
            puts(output);
        } else return 3;
    }
    return ferror(stdin) || ferror(stdout) ? 4 : 0;
}
