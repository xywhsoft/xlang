#ifndef XL_TOTP_NATIVE_H
#define XL_TOTP_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 返回独立 XRT 字符串，C 调用方用 xrtFree；失败返回 NULL 并设置 XRT 错误。 */
char* xtotp_hotp(const char* secret, long long secret_length,
    long long counter, int digits);
char* xtotp_generate_at(const char* secret, long long secret_length,
    long long timestamp, int time_step, int digits);
char* xtotp_generate(const char* secret, long long secret_length,
    int time_step, int digits);

/* 1 匹配，0 不匹配，-1 输入或内部错误；窗口限 0..10。 */
int xtotp_verify_at(const char* secret, long long secret_length,
    const char* code, long long code_length, long long timestamp,
    int time_step, int digits, int window);
int xtotp_verify(const char* secret, long long secret_length,
    const char* code, long long code_length, int time_step,
    int digits, int window);

#ifdef __cplusplus
}
#endif

#endif
