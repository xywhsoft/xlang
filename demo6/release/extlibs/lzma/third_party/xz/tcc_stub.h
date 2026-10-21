#ifndef XLZMA_TCC_STUB_H
#define XLZMA_TCC_STUB_H

/* TCC 不生成此 CPUID 子功能的内部符号。清零表示禁用可选硬件加速。 */
void Z7_FASTCALL z7_x86_cpuid_subFunc(UInt32 p[4], UInt32 func, UInt32 subFunc)
{
	(void)func;
	(void)subFunc;
	p[0] = 0;
	p[1] = 0;
	p[2] = 0;
	p[3] = 0;
}

#endif
