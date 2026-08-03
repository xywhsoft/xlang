/*
 * xbot_native.h - xbot 扩展库原生 C 层共享头文件
 *
 * XRT 调用约定：
 *   - 字符串返回: const char * (使用 xrtTempMemory 或静态缓冲区)
 *   - 整数参数/返回: long long
 *   - 浮点参数/返回: double
 *   - 原生句柄: void *
 *   - 错误报告: xrtSetError(msg)
 */
#ifndef XBOT_NATIVE_H
#define XBOT_NATIVE_H

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* XRT runtime API (provided by xlang runtime) */
extern void xrtSetError(const void *msg, int bFree);
extern void *xrtTempMemory(size_t size);

/* ─── ADB 子进程管理 (xbot_adb.c) ─── */

/*
 * xxbot_adb_run - 执行 ADB 命令 (文本输出)
 * adb_path:   adb 可执行文件路径
 * serial:     设备序列号 (可为 NULL 表示无 -s 参数)
 * args:       命令参数 (空格分隔)
 * timeout_ms: 超时毫秒数
 * 返回: "EXIT_CODE\r\nSTDOUT\r\nSTDERR" 格式字符串
 */
const char *xxbot_adb_run(const char *adb_path, const char *serial,
                           const char *args, long long timeout_ms);

/*
 * xxbot_adb_run_binary - 执行 ADB 命令 (二进制输出, 用于截图)
 * adb_path:   adb 可执行文件路径
 * serial:     设备序列号
 * args:       命令参数
 * save_path:  输出保存到的文件路径
 * timeout_ms: 超时毫秒数
 * 返回: 写入的字节数, -1 表示失败
 */
long long xxbot_adb_run_binary(const char *adb_path, const char *serial,
                                const char *args, const char *save_path,
                                long long timeout_ms);

/* ─── 加密与签名 (xbot_crypto.c) ─── */

/*
 * xxbot_sha256_hex - 计算 SHA256 哈希
 * data: 输入字符串
 * 返回: 64 字符十六进制字符串
 */
const char *xxbot_sha256_hex(const char *data);

/*
 * xxbot_hmac_sha256_hex - 计算 HMAC-SHA256
 * key:  密钥
 * data: 待签名数据
 * 返回: 64 字符十六进制字符串
 */
const char *xxbot_hmac_sha256_hex(const char *key, const char *data);

/*
 * xxbot_url_encode - URL 编码
 * value: 原始字符串
 * 返回: 编码后的字符串
 */
const char *xxbot_url_encode(const char *value);

/*
 * xxbot_read_ini_value - 读取 INI 文件配置值
 * file_path: INI 文件路径
 * section:   节名
 * key:       键名
 * 返回: 值字符串，未找到时返回空字符串
 */
const char *xxbot_read_ini_value(const char *file_path, const char *section,
                                  const char *key);

#endif /* XBOT_NATIVE_H */
