/* Test-only Native sidecar. All operands are volatile runtime reads; each
 * function must use the corrected function-kind MSVCRT import, not DATA. */
#include "static_xge_aot_math.h"
__declspec(dllimport) double sin(double);
__declspec(dllimport) double cos(double);
__declspec(dllimport) double sqrt(double);
__declspec(dllimport) double pow(double,double);
__declspec(dllimport) double fmod(double,double);
static volatile double operands[]={0.5,9.0,2.0,3.0,5.5,2.0};
int static_xge_math_probe(void)
{
    double actual[5];
    static const double expected[5]={0.479425538604203,0.8775825618903728,3.0,8.0,1.5};
    int i;
    actual[0]=sin(operands[0]);actual[1]=cos(operands[0]);
    actual[2]=sqrt(operands[1]);actual[3]=pow(operands[2],operands[3]);
    actual[4]=fmod(operands[4],operands[5]);
    for(i=0;i<5;++i){double delta=actual[i]-expected[i];
        if(!(delta>=-1e-12&&delta<=1e-12))return i+1;
    }
    return 0;
}
