/* Compiled in memory with one real DLL import per case. Runtime volatile
 * operands prohibit folding this test into a constant result. */
#if IMPORT_MATH_CASE == 0
__declspec(dllimport) double sin(double);
double import_math_entry(double first,double second){volatile double value=first;(void)second;return sin(value);}
#elif IMPORT_MATH_CASE == 1
__declspec(dllimport) double cos(double);
double import_math_entry(double first,double second){volatile double value=first;(void)second;return cos(value);}
#elif IMPORT_MATH_CASE == 2
__declspec(dllimport) double sqrt(double);
double import_math_entry(double first,double second){volatile double value=first;(void)second;return sqrt(value);}
#elif IMPORT_MATH_CASE == 3
__declspec(dllimport) double pow(double,double);
double import_math_entry(double first,double second){volatile double value=first,exponent=second;return pow(value,exponent);}
#elif IMPORT_MATH_CASE == 4
__declspec(dllimport) double fmod(double,double);
double import_math_entry(double first,double second){volatile double value=first,divisor=second;return fmod(value,divisor);}
#else
#error Invalid IMPORT_MATH_CASE
#endif
