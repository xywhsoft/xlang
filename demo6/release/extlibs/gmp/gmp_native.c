/* gmp_native.c - GMP-style big integer helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char str_buf[4096];

/* Compare two numeric strings (positive integers) */
long long xgmp_cmp(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    /* Skip leading zeros */
    while (la > 1 && a[0] == '0') { a++; la--; }
    while (lb > 1 && b[0] == '0') { b++; lb--; }
    if (la != lb) return la > lb ? 1 : -1;
    int cmp = strcmp(a, b);
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
    return 0;
}

/* Count digits in numeric string */
long long xgmp_digits(const char *num) {
    int len = (int)strlen(num);
    while (len > 1 && num[0] == '0') { num++; len--; }
    return (long long)len;
}

/* Check if numeric string is zero */
long long xgmp_is_zero(const char *num) {
    while (*num == '0' && *(num+1)) num++;
    return (*num == '0' && !*(num+1)) ? 1 : 0;
}

/* Check if numeric string is negative */
long long xgmp_is_negative(const char *num) {
    return (num[0] == '-') ? 1 : 0;
}

/* Get absolute value string */
const char *xgmp_abs(const char *num) {
    if (num[0] == '-') {
        strcpy(str_buf, num + 1);
    } else {
        strcpy(str_buf, num);
    }
    return str_buf;
}

/* Convert to base (2-36) - returns representation for small numbers */
const char *xgmp_to_base(long long value, long long base) {
    if (base < 2 || base > 36) return "INVALID";
    char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char temp[64]; int pos = 0;
    int neg = 0;
    if (value < 0) { neg = 1; value = -value; }
    if (value == 0) { temp[pos++] = '0'; }
    while (value > 0) {
        temp[pos++] = digits[value % base];
        value /= base;
    }
    int out_pos = 0;
    if (neg) str_buf[out_pos++] = '-';
    while (pos > 0) str_buf[out_pos++] = temp[--pos];
    str_buf[out_pos] = '\0';
    return str_buf;
}

/* Parse from base (2-36) - for small numbers */
long long xgmp_from_base(const char *str, long long base) {
    if (base < 2 || base > 36) return 0;
    long long result = 0;
    int neg = 0, i = 0;
    if (str[0] == '-') { neg = 1; i = 1; }
    for (; str[i]; i++) {
        int digit;
        if (str[i] >= '0' && str[i] <= '9') digit = str[i] - '0';
        else if (str[i] >= 'a' && str[i] <= 'z') digit = str[i] - 'a' + 10;
        else if (str[i] >= 'A' && str[i] <= 'Z') digit = str[i] - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
    }
    return neg ? -result : result;
}

/* GCD using Euclidean algorithm (for small numbers) */
long long xgmp_gcd(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* LCM (for small numbers) */
long long xgmp_lcm(long long a, long long b) {
    if (a == 0 || b == 0) return 0;
    long long g = xgmp_gcd(a, b);
    return (a / g) * b;
}

/* Modular exponentiation (for small numbers) */
long long xgmp_powm(long long base, long long exp, long long mod) {
    long long result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1) result = (result * base) % mod;
        exp >>= 1;
        base = (base * base) % mod;
    }
    return result;
}

/* Check if probably prime (simple trial division for small numbers) */
long long xgmp_probab_prime(long long n, long long reps) {
    if (n < 2) return 0;
    if (n == 2 || n == 3) return 1;
    if (n % 2 == 0) return 0;
    long long i;
    for (i = 3; i * i <= n && i < 1000; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

/* Factorial (for small numbers) */
long long xgmp_factorial(long long n) {
    if (n < 0) return 0;
    if (n <= 1) return 1;
    long long result = 1, i;
    for (i = 2; i <= n && i <= 20; i++) result *= i;
    return result;
}

/* Binomial coefficient C(n,k) for small numbers */
long long xgmp_binomial(long long n, long long k) {
    if (k < 0 || k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    long long result = 1, i;
    for (i = 0; i < k; i++) {
        result = result * (n - i) / (i + 1);
    }
    return result;
}

/* Version info */
long long xgmp_version_major(void) { return 6; }
long long xgmp_version_minor(void) { return 2; }
