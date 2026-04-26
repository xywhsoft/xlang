


/*
	PCG Random Number Generation for C. [ Update : 2025/08/19 from https://github.com/imneme/pcg-c-basic ]
	修改：
		整合到 xrt 库
		提供 Ex 版本 API（调用者管理状态，线程安全）
		普通版本 API 使用全局状态（线程不安全，性能优先）
	使用协议注意事项：
		Apache License, Version 2.0 协议
		允许个人使用、商业使用
		复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/



/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */



// 初始化随机数生成器
XXAPI void xrtRandSeed(xrand* rng, uint64 seed, uint64 seq)
{
	rng->state = 0U;
	rng->inc = (seq << 1u) | 1u;
	// 运算两次
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	rng->state += seed;
	oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
}



// 生成 32 位随机数 - 线程安全
XXAPI uint32 xrtRand32Ex(xrand* rng)
{
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	uint32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32 rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}



// 生成 64 位随机数 - 线程安全
XXAPI uint64 xrtRand64Ex(xrand* rngLow, xrand* rngHigh)
{
	uint32 iLow = xrtRand32Ex(rngLow);
	uint32 iHigh = xrtRand32Ex(rngHigh);
	return ((uint64)iHigh << 32) | (uint64)iLow;
}



// 生成范围随机数 - 线程安全
XXAPI int xrtRandRangeEx(xrand* rng, int min, int max)
{
	uint32 iRange = (max - min) + 1;
	if ( iRange > 0 ) {
		uint32 threshold = -iRange % iRange;
		for (;;) {
			uint32 r = xrtRand32Ex(rng);
			if (r >= threshold)
				return (r % iRange) + min;
		}
	} else if ( iRange < 0 ) {
		iRange = (min - max) + 1;
		uint32 threshold = -iRange % iRange;
		for (;;) {
			uint32 r = xrtRand32Ex(rng);
			if (r >= threshold)
				return (r % iRange) + max;
		}
	}
	return 0;
}



// 获取 32 位随机数
XXAPI uint32 xrtRand32()
{
	return xrtRand32Ex(&xCore.rand32);
}



// 获取 64 位随机数
XXAPI uint64 xrtRand64()
{
	return xrtRand64Ex(&xCore.rand64_low, &xCore.rand64_high);
}



// 获取 32 位范围随机数
XXAPI int xrtRandRange(int min, int max)
{
	return xrtRandRangeEx(&xCore.rand32, min, max);
}



// 整数约等于
XXAPI bool xrtIntApprox(int64 a, int64 b)
{
	if ( a == b ) { return TRUE; }
	
	int64 diff = (a > b) ? (a - b) : (b - a);
	
	if ( xCore.iApproxIntMode == XRT_APPROX_PERCENT ) {
		// 百分比模式
		if ( xCore.fApproxIntTol <= 0.0 ) { return FALSE; }
		int64 maxAbs = (a >= 0 ? a : -a);
		int64 bAbs = (b >= 0 ? b : -b);
		if ( bAbs > maxAbs ) { maxAbs = bAbs; }
		if ( maxAbs == 0 ) { return TRUE; }
		double ratio = (double)diff / (double)maxAbs;
		return (ratio <= xCore.fApproxIntTol);
	} else {
		// 差值模式
		return (diff <= (int64)xCore.fApproxIntTol);
	}
}



// 浮点数约等于
XXAPI bool xrtNumApprox(double a, double b)
{
	if ( a == b ) { return TRUE; }
	
	double diff = (a > b) ? (a - b) : (b - a);
	
	if ( xCore.iApproxNumMode == XRT_APPROX_PERCENT ) {
		// 百分比模式
		if ( xCore.fApproxNumTol <= 0.0 ) { return FALSE; }
		double maxAbs = (a >= 0.0 ? a : -a);
		double bAbs = (b >= 0.0 ? b : -b);
		if ( bAbs > maxAbs ) { maxAbs = bAbs; }
		if ( maxAbs == 0.0 ) { return TRUE; }
		double ratio = diff / maxAbs;
		return (ratio <= xCore.fApproxNumTol);
	} else {
		// 差值模式
		return (diff <= xCore.fApproxNumTol);
	}
}

