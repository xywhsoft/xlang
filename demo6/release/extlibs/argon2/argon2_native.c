/*
 * xlang Argon2 wrapper.
 *
 * 算法实现来自 Argon2 官方参考库；本文件只负责 XRT 资源接入和 xlang ABI。
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "vendor/argon2.h"
#include "vendor/encoding.h"

extern void xrtSetError(const void *message, int freeMessage);
extern void *xrtTempMemory(size_t size);
extern void *xrtMalloc(size_t size);
extern void xrtFree(void *memory);
extern int xrtSecureRandom(void *data, size_t size);
extern void xrtSecureZero(void *data, size_t size);

#define XARGON2_MAX_HASH_LENGTH 256
#define XARGON2_MAX_SALT_LENGTH 64
#define XARGON2_MAX_ENCODED_LENGTH 4096



static void xargon2_set_error(const char *message)
{
    xrtSetError(message, 0);
}



static void xargon2_set_error_code(int code)
{
    const char *message = argon2_error_message(code);

    xargon2_set_error(message != NULL ? message : "argon2 operation failed");
}



static int xargon2_allocate(uint8_t **memory, size_t size)
{
    *memory = (uint8_t *)xrtMalloc(size);
    return *memory != NULL ? 0 : -1;
}



static void xargon2_free(uint8_t *memory, size_t size)
{
    if ( memory == NULL ) {
        return;
    }
    xrtSecureZero(memory, size);
    xrtFree(memory);
}



static int xargon2_type_from_value(long long value, argon2_type *type)
{
    if ( value < (long long)Argon2_d || value > (long long)Argon2_id ) {
        return 0;
    }
    *type = (argon2_type)value;
    return 1;
}



static int xargon2_type_from_encoded(const char *encoded, argon2_type *type)
{
    if ( encoded == NULL ) {
        return 0;
    }
    if ( strncmp(encoded, "$argon2id$", 10) == 0 ) {
        *type = Argon2_id;
        return 1;
    }
    if ( strncmp(encoded, "$argon2i$", 9) == 0 ) {
        *type = Argon2_i;
        return 1;
    }
    if ( strncmp(encoded, "$argon2d$", 9) == 0 ) {
        *type = Argon2_d;
        return 1;
    }
    return 0;
}



static int xargon2_validate_costs(long long timeCost, long long memoryCost,
                                  long long parallelism,
                                  long long hashLength,
                                  long long saltLength)
{
    if ( timeCost < 1 || timeCost > (long long)UINT32_MAX ||
         memoryCost < 1 || memoryCost > (long long)UINT32_MAX ||
         parallelism < 1 || parallelism > (long long)UINT32_MAX ) {
        xargon2_set_error("argon2 costs must be positive 32-bit values");
        return 0;
    }
    if ( hashLength < ARGON2_MIN_OUTLEN ||
         hashLength > XARGON2_MAX_HASH_LENGTH ) {
        xargon2_set_error("argon2 hash length must be between 4 and 256 bytes");
        return 0;
    }
    if ( saltLength < ARGON2_MIN_SALT_LENGTH ||
         saltLength > XARGON2_MAX_SALT_LENGTH ) {
        xargon2_set_error("argon2 salt length must be between 8 and 64 bytes");
        return 0;
    }
    return 1;
}



static void xargon2_context_init(argon2_context *context,
                                 uint8_t *output, uint32_t outputLength,
                                 const char *password,
                                 const uint8_t *salt, uint32_t saltLength,
                                 const char *secret, const char *ad,
                                 uint32_t timeCost, uint32_t memoryCost,
                                 uint32_t parallelism)
{
    memset(context, 0, sizeof(*context));
    context->out = output;
    context->outlen = outputLength;
    context->pwd = (uint8_t *)(password != NULL ? password : "");
    context->pwdlen = (uint32_t)strlen(password != NULL ? password : "");
    context->salt = (uint8_t *)salt;
    context->saltlen = saltLength;
    context->secret = (uint8_t *)(secret != NULL ? secret : "");
    context->secretlen = (uint32_t)strlen(secret != NULL ? secret : "");
    context->ad = (uint8_t *)(ad != NULL ? ad : "");
    context->adlen = (uint32_t)strlen(ad != NULL ? ad : "");
    context->t_cost = timeCost;
    context->m_cost = memoryCost;
    context->lanes = parallelism;
    context->threads = parallelism;
    context->version = ARGON2_VERSION_13;
    context->allocate_cbk = xargon2_allocate;
    context->free_cbk = xargon2_free;
    context->flags = ARGON2_DEFAULT_FLAGS;
}



static int xargon2_hash_bytes(uint8_t *output, uint32_t outputLength,
                              const char *password,
                              const uint8_t *salt, uint32_t saltLength,
                              const char *secret, const char *ad,
                              uint32_t timeCost, uint32_t memoryCost,
                              uint32_t parallelism, argon2_type type,
                              argon2_context *context)
{
    xargon2_context_init(context, output, outputLength, password,
                         salt, saltLength, secret, ad,
                         timeCost, memoryCost, parallelism);
    return argon2_ctx(context, type);
}



const char *xargon2_hash(const char *password, const char *salt,
                         const char *secret, const char *ad,
                         long long timeCost, long long memoryCost,
                         long long parallelism, long long hashLength,
                         long long saltLength, long long typeValue)
{
    uint8_t output[XARGON2_MAX_HASH_LENGTH];
    uint8_t automaticSalt[XARGON2_MAX_SALT_LENGTH];
    const uint8_t *actualSalt;
    uint32_t actualSaltLength;
    argon2_context context;
    argon2_type type;
    size_t encodedLength;
    char *encoded;
    int result;
    int automatic = salt == NULL || salt[0] == '\0';

    if ( !xargon2_type_from_value(typeValue, &type) ||
         !xargon2_validate_costs(timeCost, memoryCost, parallelism,
                                 hashLength, saltLength) ) {
        if ( typeValue < 0 || typeValue > 2 ) {
            xargon2_set_error("argon2 type must be ARGON2D, ARGON2I or ARGON2ID");
        }
        return "";
    }

    if ( automatic ) {
        actualSaltLength = (uint32_t)saltLength;
        if ( !xrtSecureRandom(automaticSalt, actualSaltLength) ) {
            return "";
        }
        actualSalt = automaticSalt;
    } else {
        size_t length = strlen(salt);

        if ( length < ARGON2_MIN_SALT_LENGTH || length > UINT32_MAX ) {
            xargon2_set_error("argon2 salt must contain at least 8 bytes");
            return "";
        }
        actualSalt = (const uint8_t *)salt;
        actualSaltLength = (uint32_t)length;
    }

    result = xargon2_hash_bytes(output, (uint32_t)hashLength,
                                password, actualSalt, actualSaltLength,
                                secret, ad, (uint32_t)timeCost,
                                (uint32_t)memoryCost,
                                (uint32_t)parallelism, type, &context);
    if ( result != ARGON2_OK ) {
        if ( automatic ) {
            xrtSecureZero(automaticSalt, sizeof(automaticSalt));
        }
        xrtSecureZero(output, sizeof(output));
        xargon2_set_error_code(result);
        return "";
    }

    encodedLength = argon2_encodedlen((uint32_t)timeCost,
                                      (uint32_t)memoryCost,
                                      (uint32_t)parallelism,
                                      actualSaltLength,
                                      (uint32_t)hashLength, type);
    encoded = (char *)xrtTempMemory(encodedLength);
    if ( encoded == NULL ) {
        if ( automatic ) {
            xrtSecureZero(automaticSalt, sizeof(automaticSalt));
        }
        xrtSecureZero(output, sizeof(output));
        xargon2_set_error("argon2 output allocation failed");
        return "";
    }
    result = encode_string(encoded, encodedLength, &context, type);
    if ( automatic ) {
        xrtSecureZero(automaticSalt, sizeof(automaticSalt));
    }
    xrtSecureZero(output, sizeof(output));
    if ( result != ARGON2_OK ) {
        xargon2_set_error_code(result);
        return "";
    }
    return encoded;
}



const char *xargon2_hash_raw(const char *password, const char *salt,
                             const char *secret, const char *ad,
                             long long timeCost, long long memoryCost,
                             long long parallelism, long long hashLength,
                             long long saltLength, long long typeValue)
{
    static const char digits[] = "0123456789abcdef";
    uint8_t output[XARGON2_MAX_HASH_LENGTH];
    uint8_t automaticSalt[XARGON2_MAX_SALT_LENGTH];
    const uint8_t *actualSalt;
    uint32_t actualSaltLength;
    argon2_context context;
    argon2_type type;
    char *hex;
    size_t i;
    int result;
    int automatic = salt == NULL || salt[0] == '\0';

    if ( !xargon2_type_from_value(typeValue, &type) ||
         !xargon2_validate_costs(timeCost, memoryCost, parallelism,
                                 hashLength, saltLength) ) {
        if ( typeValue < 0 || typeValue > 2 ) {
            xargon2_set_error("argon2 type must be ARGON2D, ARGON2I or ARGON2ID");
        }
        return "";
    }

    if ( automatic ) {
        actualSaltLength = (uint32_t)saltLength;
        if ( !xrtSecureRandom(automaticSalt, actualSaltLength) ) {
            return "";
        }
        actualSalt = automaticSalt;
    } else {
        size_t length = strlen(salt);

        if ( length < ARGON2_MIN_SALT_LENGTH || length > UINT32_MAX ) {
            xargon2_set_error("argon2 salt must contain at least 8 bytes");
            return "";
        }
        actualSalt = (const uint8_t *)salt;
        actualSaltLength = (uint32_t)length;
    }

    result = xargon2_hash_bytes(output, (uint32_t)hashLength,
                                password, actualSalt, actualSaltLength,
                                secret, ad, (uint32_t)timeCost,
                                (uint32_t)memoryCost,
                                (uint32_t)parallelism, type, &context);
    if ( automatic ) {
        xrtSecureZero(automaticSalt, sizeof(automaticSalt));
    }
    if ( result != ARGON2_OK ) {
        xrtSecureZero(output, sizeof(output));
        xargon2_set_error_code(result);
        return "";
    }

    hex = (char *)xrtTempMemory((size_t)hashLength * 2u + 1u);
    if ( hex == NULL ) {
        xrtSecureZero(output, sizeof(output));
        xargon2_set_error("argon2 output allocation failed");
        return "";
    }
    for ( i = 0; i < (size_t)hashLength; i++ ) {
        hex[i * 2u] = digits[output[i] >> 4];
        hex[i * 2u + 1u] = digits[output[i] & 15u];
    }
    hex[(size_t)hashLength * 2u] = '\0';
    xrtSecureZero(output, sizeof(output));
    return hex;
}



static int xargon2_decode(const char *encoded, const char *password,
                          argon2_context *context, argon2_type *type,
                          uint8_t **salt, uint8_t **output)
{
    size_t length;
    int result;

    if ( !xargon2_type_from_encoded(encoded, type) ) {
        return ARGON2_DECODING_FAIL;
    }
    length = strlen(encoded);
    if ( length == 0 || length > XARGON2_MAX_ENCODED_LENGTH ) {
        return ARGON2_DECODING_LENGTH_FAIL;
    }

    *salt = (uint8_t *)xrtMalloc(length);
    *output = (uint8_t *)xrtMalloc(length);
    if ( *salt == NULL || *output == NULL ) {
        xargon2_free(*salt, length);
        xargon2_free(*output, length);
        *salt = NULL;
        *output = NULL;
        return ARGON2_MEMORY_ALLOCATION_ERROR;
    }

    memset(context, 0, sizeof(*context));
    context->salt = *salt;
    context->saltlen = (uint32_t)length;
    context->out = *output;
    context->outlen = (uint32_t)length;
    context->pwd = (uint8_t *)(password != NULL ? password : "");
    context->pwdlen = (uint32_t)strlen(password != NULL ? password : "");
    result = decode_string(context, encoded, *type);
    if ( result != ARGON2_OK ) {
        xargon2_free(*salt, length);
        xargon2_free(*output, length);
        *salt = NULL;
        *output = NULL;
    }
    return result;
}



long long xargon2_verify(const char *password, const char *encoded)
{
    argon2_context context;
    argon2_type type;
    uint8_t *salt = NULL;
    uint8_t *expected = NULL;
    uint8_t *computed = NULL;
    uint32_t outputLength;
    size_t encodedLength;
    uint8_t difference = 0;
    uint32_t i;
    int result;

    result = xargon2_decode(encoded, password, &context, &type,
                            &salt, &expected);
    if ( result != ARGON2_OK ) {
        return 0;
    }
    encodedLength = strlen(encoded);
    outputLength = context.outlen;
    computed = (uint8_t *)xrtMalloc(outputLength);
    if ( computed == NULL ) {
        xargon2_free(salt, encodedLength);
        xargon2_free(expected, encodedLength);
        xargon2_set_error("argon2 verification allocation failed");
        return 0;
    }

    context.out = computed;
    context.allocate_cbk = xargon2_allocate;
    context.free_cbk = xargon2_free;
    result = argon2_ctx(&context, type);
    if ( result == ARGON2_OK ) {
        for ( i = 0; i < outputLength; i++ ) {
            difference |= (uint8_t)(computed[i] ^ expected[i]);
        }
    }

    xargon2_free(computed, outputLength);
    xargon2_free(salt, encodedLength);
    xargon2_free(expected, encodedLength);
    if ( result != ARGON2_OK ) {
        xargon2_set_error_code(result);
        return 0;
    }
    return difference == 0 ? 1 : 0;
}



long long xargon2_needs_rehash(const char *encoded,
                               long long recommendedTime,
                               long long recommendedMemory,
                               long long recommendedParallelism)
{
    argon2_context context;
    argon2_type type;
    uint8_t *salt = NULL;
    uint8_t *output = NULL;
    size_t encodedLength;
    int result = xargon2_decode(encoded, "", &context, &type,
                                &salt, &output);
    int needs;

    if ( result != ARGON2_OK ) {
        return 1;
    }
    encodedLength = strlen(encoded);
    needs = context.version != ARGON2_VERSION_13 ||
            context.t_cost != (uint32_t)recommendedTime ||
            context.m_cost != (uint32_t)recommendedMemory ||
            context.lanes != (uint32_t)recommendedParallelism;
    xargon2_free(salt, encodedLength);
    xargon2_free(output, encodedLength);
    return needs ? 1 : 0;
}



const char *xargon2_extract(const char *encoded)
{
    argon2_context context;
    argon2_type type;
    uint8_t *salt = NULL;
    uint8_t *output = NULL;
    size_t encodedLength;
    char *resultText;
    int result = xargon2_decode(encoded, "", &context, &type,
                                &salt, &output);

    if ( result != ARGON2_OK ) {
        return "";
    }
    resultText = (char *)xrtTempMemory(96);
    if ( resultText == NULL ) {
        encodedLength = strlen(encoded);
        xargon2_free(salt, encodedLength);
        xargon2_free(output, encodedLength);
        return "";
    }
    snprintf(resultText, 96, "%d,%u,%u,%u,%u,%u", (int)type,
             context.t_cost, context.m_cost, context.lanes,
             context.outlen, context.saltlen);
    encodedLength = strlen(encoded);
    xargon2_free(salt, encodedLength);
    xargon2_free(output, encodedLength);
    return resultText;
}



const char *xargon2_generate_salt(long long length)
{
    static const char digits[] = "0123456789abcdef";
    uint8_t bytes[XARGON2_MAX_SALT_LENGTH];
    char *hex;
    size_t i;

    if ( length < ARGON2_MIN_SALT_LENGTH ||
         length > XARGON2_MAX_SALT_LENGTH ) {
        xargon2_set_error("argon2 salt length must be between 8 and 64 bytes");
        return "";
    }
    if ( !xrtSecureRandom(bytes, (size_t)length) ) {
        return "";
    }
    hex = (char *)xrtTempMemory((size_t)length * 2u + 1u);
    if ( hex == NULL ) {
        xrtSecureZero(bytes, sizeof(bytes));
        xargon2_set_error("argon2 salt allocation failed");
        return "";
    }
    for ( i = 0; i < (size_t)length; i++ ) {
        hex[i * 2u] = digits[bytes[i] >> 4];
        hex[i * 2u + 1u] = digits[bytes[i] & 15u];
    }
    hex[(size_t)length * 2u] = '\0';
    xrtSecureZero(bytes, sizeof(bytes));
    return hex;
}



long long xargon2_recommended_time(void) { return 3; }
long long xargon2_recommended_memory(void) { return 65536; }
long long xargon2_recommended_parallelism(void) { return 4; }
long long xargon2_recommended_hashlen(void) { return 32; }
long long xargon2_recommended_saltlen(void) { return 16; }
