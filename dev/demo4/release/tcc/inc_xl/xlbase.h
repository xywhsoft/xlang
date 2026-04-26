


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>



// 引入 xrt 头文件（声明）
#include "inline_xrt.h"



// xlang 全局数据指针（inline_xrt.h 已定义 xCore）



// 接收主程序传递的全局数据
// 使用 extern 可能导致数据指针出现变化，原因不明，先用这样的方式传递全局数据
void XL_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		xCore = ptr;
	}
}


