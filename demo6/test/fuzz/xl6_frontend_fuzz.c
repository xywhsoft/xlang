#include "src/parser/xl6_parser_api.h"

#include "lib/xrt/xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XL6_FUZZ_SOURCE_CAPACITY 8192u

typedef struct XL6_FuzzRandom {
    unsigned int state;
} XL6_FuzzRandom;

static unsigned int xl6FuzzNext(XL6_FuzzRandom* random)
{
    unsigned int value;

    value = random->state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random->state = value != 0u ? value : 0x9E3779B9u;
    return random->state;
}

static size_t xl6FuzzChoose(XL6_FuzzRandom* random, size_t limit)
{
    return limit == 0u ? 0u : (size_t)(xl6FuzzNext(random) % (unsigned int)limit);
}

static unsigned char xl6FuzzByte(XL6_FuzzRandom* random)
{
    static const unsigned char alphabet[] = {
        ' ', '\t', '\n', '\r', '/', '*', '#', '"', '\'', '$', '{', '}',
        '(', ')', '[', ']', '<', '>', ':', ';', ',', '.', '=', '+', '-',
        '0', '1', '9', '_', 'a', 'Z', 0x7Fu, 0x80u, 0xBFu, 0xC0u, 0xE4u, 0xFFu
    };

    return alphabet[xl6FuzzChoose(random, sizeof(alphabet))];
}

static size_t xl6FuzzSeed(char* out, size_t capacity, XL6_FuzzRandom* random)
{
    static const char* seeds[] = {
        "func main(...) as int\n    return 0\nend\n",
        "class Box<T is comparable>\n    T value\nend\n",
        "auto text = rf\"value={1 + 2}\"\n",
        "/* outer /* nested */ comment */\nint value = 1\n",
        "try\n    throw \"boom\"\ncatch message\n    print(message)\nfinally\n    assert true\nend\n",
        "func pair<T>(T value) as (bool, T)\n    return true, value\nend\n",
        "for item in [1, 2, 3]\n    print(item)\nend\n",
        "protocol Named\n    func name() as string\nend\nclass User\n    func name() as string\n        return \"x\"\n    end\nend\n",
        "enum Status as uint8\n    ready = 1\n    failed\nend\n",
        "enum Result<T>\n    none\n    some(T value)\nend\nfunc read(Result<int> value) as int\n    return match value\n    case none then 0\n    case some(item) then item\n    end\nend\n",
        "class Node\n    int value = 0\nend\nNode? maybe = null\nweak<Node> observer = null\nint value = maybe?.value ?? 0\n",
        "#doc\ndesc:\n    parser fuzz seed\n#end\nfunc probe()\nend\n"
    };
    const char* seed;
    size_t length;

    seed = seeds[xl6FuzzChoose(random, sizeof(seeds) / sizeof(seeds[0]))];
    length = strlen(seed);
    if ( length >= capacity ) {
        length = capacity - 1u;
    }
    memcpy(out, seed, length);
    out[length] = '\0';
    return length;
}

static void xl6FuzzMutate(char* text, size_t capacity, size_t* length, XL6_FuzzRandom* random)
{
    size_t operationCount;
    size_t operation;

    operationCount = 1u + xl6FuzzChoose(random, 24u);
    for ( operation = 0; operation < operationCount; ++operation ) {
        size_t index;
        size_t action;

        action = xl6FuzzChoose(random, 4u);
        index = xl6FuzzChoose(random, *length + 1u);
        if ( action == 0u && *length > 0u ) {
            text[index < *length ? index : *length - 1u] = (char)xl6FuzzByte(random);
        } else if ( action == 1u && *length + 1u < capacity ) {
            memmove(text + index + 1u, text + index, *length - index + 1u);
            text[index] = (char)xl6FuzzByte(random);
            *length += 1u;
        } else if ( action == 2u && *length > 0u ) {
            index = index < *length ? index : *length - 1u;
            memmove(text + index, text + index + 1u, *length - index);
            *length -= 1u;
        } else if ( *length + 4u < capacity ) {
            static const char fragment[] = "{}\n";
            memmove(text + index + sizeof(fragment) - 1u, text + index, *length - index + 1u);
            memcpy(text + index, fragment, sizeof(fragment) - 1u);
            *length += sizeof(fragment) - 1u;
        }
    }
    text[*length] = '\0';
}

static int xl6FuzzOne(const char* source)
{
    XL6_ParseResult result;

    xl6ParseResultInit(&result);
    (void)xl6ParseSource(source, "<frontend-fuzz>", &result);
    xl6ParseResultFree(&result);
    return 1;
}

int main(int argc, char** argv)
{
    XL6_FuzzRandom random;
    unsigned int iterations;
    unsigned int i;
    char* source;

    iterations = argc > 1 ? (unsigned int)strtoul(argv[1], NULL, 10) : 10000u;
    if ( iterations == 0u ) {
        iterations = 1u;
    }
    random.state = argc > 2 ? (unsigned int)strtoul(argv[2], NULL, 0) : 0x584C3601u;
    if ( random.state == 0u ) {
        random.state = 0x584C3601u;
    }

    if ( !xrtInit() ) {
        fputs("xrtInit failed\n", stderr);
        return 2;
    }
    source = (char*)xrtMalloc(XL6_FUZZ_SOURCE_CAPACITY);
    if ( source == NULL ) {
        xrtUnit();
        return 3;
    }

    for ( i = 0u; i < iterations; ++i ) {
        size_t length;

        length = xl6FuzzSeed(source, XL6_FUZZ_SOURCE_CAPACITY, &random);
        xl6FuzzMutate(source, XL6_FUZZ_SOURCE_CAPACITY, &length, &random);
        if ( !xl6FuzzOne(source) ) {
            xrtFree(source);
            xrtUnit();
            return 4;
        }
    }

    xrtFree(source);
    xrtUnit();
    printf("frontend fuzz ok: %u iterations\n", iterations);
    return 0;
}
