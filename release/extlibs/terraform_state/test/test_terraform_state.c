/* 原始 tfstate 的 C 边界：结果所有权、结构校验与查询。 */
#define XRT_MODULE_ALL
#include "terraform_state.h"

#include <stdio.h>
#include <string.h>

static int fail(cstr sMessage)
{
    fprintf(stderr, "terraform_state C smoke failed: %s\n", sMessage);
    return 1;
}

static bool text_is(const xvalue* pValue, cstr sExpected)
{
    xstrview Text;
    size_t iSize = strlen(sExpected);

    return xrtValueGetString(pValue, &Text) && Text.Size == iSize &&
        memcmp(Text.Data, sExpected, iSize) == 0;
}

static int check_validation_memory(cstr sState)
{
    xvalue* pText = xrtValueString((xstrview){ sState, strlen(sState) });
    unsigned failure;
    if (pText == NULL) return fail("validation fixture allocation");
    for (failure = 0u; failure < 16u; ++failure) {
        bool valid;
        bool triggered;
        xrtClearError();
        if (!xrtMemDebugFailAfter(failure)) return fail("start validation fault");
        valid = xtf_valid(sState);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if (valid || xrtGetError() == NULL ||
                xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                return fail("valid text lost memory error");
        } else if (!valid || xrtGetError() != NULL) {
            return fail("valid text rejected");
        }

        xrtClearError();
        if (!xrtMemDebugFailAfter(failure)) return fail("start value fault");
        valid = xtf_valid_value(pText);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if (valid || xrtGetError() == NULL ||
                xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                return fail("valid value lost memory error");
        } else if (!valid || xrtGetError() != NULL) {
            return fail("valid value rejected");
        }
    }
    xrtClearError();
    xrtValueRelease(pText);
    return 0;
}

static int check_query_ownership(cstr sState)
{
    xvalue* pState = xtf_parse(sState);
    xvalue* pFound;
    xvalue* pInstances;
    xvalue* pAttributes;
    xvalue* pOutputs;
    xvalue* pOutput;
    bool valid;

    if (pState == NULL) return fail("ownership fixture parse");
    pFound = xtf_find(pState, "aws_instance.web");
    pInstances = xtf_instances(pState, "aws_instance.web");
    pAttributes = xtf_attributes(pState, "aws_instance.web", 0);
    pOutputs = xtf_outputs(pState);
    pOutput = xtf_output(pState, "answer");
    xrtValueRelease(pState);

    valid = pFound != NULL && pInstances != NULL && pAttributes != NULL &&
        pOutputs != NULL && pOutput != NULL &&
        text_is(xrtValueObjectGet(pFound, XRT_STR_LITERAL("name")), "web") &&
        xrtValueCount(pInstances) == 1u &&
        text_is(xrtValueObjectGet(pAttributes, XRT_STR_LITERAL("id")), "i-c") &&
        xrtValueObjectGet(pOutputs, XRT_STR_LITERAL("answer")) != NULL &&
        xrtValueObjectGet(pOutput, XRT_STR_LITERAL("value")) != NULL;
    xrtValueRelease(pFound);
    xrtValueRelease(pInstances);
    xrtValueRelease(pAttributes);
    xrtValueRelease(pOutputs);
    xrtValueRelease(pOutput);
    return valid ? 0 : fail("query result did not outlive source state");
}

static int check_query_memory(cstr sState)
{
    xvalue* pState = xtf_parse(sState);
    int api;
    if (pState == NULL) return fail("query fault fixture parse");
    for (api = 0; api < 2; ++api) {
        unsigned failure;
        unsigned rejected = 0u, succeeded = 0u;
        for (failure = 0u; failure < 16u; ++failure) {
            xvalue* pResult;
            bool triggered;
            xrtClearError();
            if (!xrtMemDebugFailAfter(failure)) {
                xrtValueRelease(pState);
                return fail("start query fault");
            }
            pResult = api == 0 ? xtf_addresses(pState) :
                xtf_find(pState, "aws_instance.web");
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                bool valid = pResult == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY;
                xrtValueRelease(pResult);
                if (!valid) {
                    xrtValueRelease(pState);
                    return fail("query lost allocation error");
                }
                ++rejected;
            } else {
                bool valid = pResult != NULL && xrtGetError() == NULL &&
                    (api == 0 ?
                        xrtValueCount(pResult) == 1u &&
                        text_is(xrtValueArrayGet(pResult, 0u), "aws_instance.web") :
                        text_is(xrtValueObjectGet(pResult,
                            XRT_STR_LITERAL("name")), "web"));
                xrtValueRelease(pResult);
                if (!valid) {
                    xrtValueRelease(pState);
                    return fail("query rejected valid state");
                }
                ++succeeded;
            }
            xrtClearError();
            pResult = xtf_find(pState, "aws_instance.web");
            if (pResult == NULL ||
                !text_is(xrtValueObjectGet(pResult, XRT_STR_LITERAL("name")), "web")) {
                xrtValueRelease(pResult);
                xrtValueRelease(pState);
                return fail("query did not recover after allocation fault");
            }
            xrtValueRelease(pResult);
        }
        if (rejected == 0u || succeeded == 0u) {
            xrtValueRelease(pState);
            return fail("query fault points were not exercised");
        }
    }
    xrtValueRelease(pState);
    return 0;
}

/* 重复资源与不同字段拼出的同一地址都不能让查询静默命中第一项。 */
static int check_duplicate_addresses(void)
{
    cstr cases[] = {
        "{\"version\":4,\"serial\":0,\"lineage\":\"x\",\"outputs\":{},\"resources\":["
        "{\"type\":\"aws_instance\",\"name\":\"web\"},"
        "{\"type\":\"aws_instance\",\"name\":\"web\"}]}",
        "{\"version\":4,\"serial\":0,\"lineage\":\"x\",\"outputs\":{},\"resources\":["
        "{\"type\":\"a.b\",\"name\":\"c\"},"
        "{\"type\":\"a\",\"name\":\"b.c\"}]}"
    };
    cstr addresses[] = { "aws_instance.web", "a.b.c" };
    size_t i;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        xvalue* state = xtf_parse(cases[i]);
        xvalue* result;
        if (state == NULL) return fail("duplicate fixture parse");
        result = xtf_addresses(state);
        if (result != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtValueRelease(result);
            xrtValueRelease(state);
            return fail("addresses accepted duplicate resource address");
        }
        result = xtf_find(state, addresses[i]);
        if (result != NULL || xrtGetError() == NULL ||
            xrtErrorKind(xrtGetError()) != XERR_VALUE) {
            xrtValueRelease(result);
            xrtValueRelease(state);
            return fail("find returned an ambiguous resource");
        }
        xrtValueRelease(state);
    }
    return 0;
}

int main(void)
{
    cstr sState = "{\"version\":4,\"serial\":9,\"lineage\":\"c-smoke\","
        "\"outputs\":{\"answer\":{\"value\":42}},\"resources\":["
        "{\"type\":\"aws_instance\",\"name\":\"web\",\"instances\":["
        "{\"attributes\":{\"id\":\"i-c\"}}]}]}";
    xvalue* pState;
    xvalue* pAddresses;
    xvalue* pAttributes;
    xvalue* pOutput;
    char* sLineage = NULL;
    int64 iAnswer;

    if (check_validation_memory(sState) != 0) return 1;
    if (check_query_ownership(sState) != 0) return 1;
    if (check_query_memory(sState) != 0) return 1;
    if (check_duplicate_addresses() != 0) return 1;
    pState = xtf_parse(sState);

    if (pState == NULL || !xtf_valid(sState) || xtf_valid("[]") ||
        xrtGetError() != NULL ||
        xtf_version(pState) != 4 || xtf_serial(pState) != 9 ||
        (sLineage = xtf_lineage(pState)) == NULL ||
        strcmp(sLineage, "c-smoke") != 0) {
        xrtFree(sLineage);
        xrtValueRelease(pState);
        return fail("state metadata");
    }
    xrtFree(sLineage);
    sLineage = xtf_lineage(pState);
    if (sLineage == NULL || !xtf_valid(sState) || strcmp(sLineage, "c-smoke") != 0) {
        xrtFree(sLineage);
        xrtValueRelease(pState);
        return fail("lineage result must survive another call");
    }
    xrtFree(sLineage);
    pAddresses = xtf_addresses(pState);
    pAttributes = xtf_attributes(pState, "aws_instance.web", 0);
    pOutput = xtf_output(pState, "answer");
    if (pAddresses == NULL || xrtValueCount(pAddresses) != 1u ||
        !text_is(xrtValueArrayGet(pAddresses, 0u), "aws_instance.web") ||
        pAttributes == NULL || !text_is(xrtValueObjectGet(pAttributes, XRT_STR_LITERAL("id")), "i-c") ||
        pOutput == NULL || !xrtValueGetInt(xrtValueObjectGet(pOutput, XRT_STR_LITERAL("value")), &iAnswer) ||
        iAnswer != 42) {
        xrtValueRelease(pAddresses);
        xrtValueRelease(pAttributes);
        xrtValueRelease(pOutput);
        xrtValueRelease(pState);
        return fail("resource queries");
    }
    {
        xvalue* pMissing = xtf_find(pState, "missing");
        if (pMissing == NULL || xrtValueType(pMissing) != XVALUE_NULL) {
            xrtValueRelease(pMissing);
            xrtValueRelease(pAddresses);
            xrtValueRelease(pAttributes);
            xrtValueRelease(pOutput);
            xrtValueRelease(pState);
            return fail("missing resource");
        }
        xrtValueRelease(pMissing);
    }
    {
        static const char with_nul[] = "{\"version\":4,\"serial\":0,\"lineage\":\"x\","
            "\"resources\":[],\"outputs\":{}}\0garbage";
        const char* invalid_states[] = {
            "{}", "{\"version\":3,\"serial\":0,\"lineage\":\"x\",\"resources\":[],\"outputs\":{}}",
            "{\"version\":4,\"serial\":-1,\"lineage\":\"x\",\"resources\":[],\"outputs\":{}}",
            "{\"version\":4,\"serial\":0,\"lineage\":\"x\",\"resources\":{},\"outputs\":{}}",
            "{\"version\":4,\"serial\":0,\"lineage\":\"x\\u0000hidden\",\"resources\":[],\"outputs\":{}}",
            "{\"format_version\":\"1.0\",\"values\":{}}"
        };
        size_t i;
        xvalue* pText;
        xvalue* pParsed;
        for (i = 0; i < sizeof(invalid_states) / sizeof(invalid_states[0]); ++i) {
            pParsed = xtf_parse(invalid_states[i]);
            if (pParsed != NULL || xtf_valid(invalid_states[i])) {
                xrtValueRelease(pParsed);
                return fail("invalid state accepted");
            }
        }
        pParsed = xtf_parse_len(with_nul, sizeof(with_nul) - 1u);
        if (pParsed != NULL) {
            xrtValueRelease(pParsed);
            return fail("embedded NUL accepted");
        }
        pText = xrtValueString((xstrview){ sState, strlen(sState) });
        pParsed = xtf_parse_value(pText);
        if (pParsed == NULL || !xtf_valid_value(pText)) {
            xrtValueRelease(pParsed);
            xrtValueRelease(pText);
            return fail("length-aware state parse");
        }
        xrtValueRelease(pParsed);
        xrtValueRelease(pText);
        pText = xrtValueString((xstrview){ with_nul, sizeof(with_nul) - 1u });
        pParsed = xtf_parse_value(pText);
        if (pParsed != NULL || xtf_valid_value(pText) || xrtGetError() != NULL) {
            xrtValueRelease(pParsed);
            xrtValueRelease(pText);
            return fail("length-aware parse ignored embedded NUL");
        }
        xrtValueRelease(pText);
        pParsed = xtf_parse("{\"version\":4,\"serial\":0,\"lineage\":\"x\","
            "\"resources\":[{\"mode\":\"other\",\"type\":\"x\",\"name\":\"y\"}],\"outputs\":{}}");
        if (pParsed == NULL) return fail("invalid resource fixture");
        {
            xvalue* pInvalidAddresses = xtf_addresses(pParsed);
            if (pInvalidAddresses != NULL || xrtGetError() == NULL) {
                xrtValueRelease(pInvalidAddresses);
                xrtValueRelease(pParsed);
                return fail("invalid resource mode accepted");
            }
            xrtClearError();
        }
        xrtValueRelease(pParsed);
        pParsed = xtf_attributes(pState, "aws_instance.web", -1);
        if (pParsed != NULL || xrtGetError() == NULL) {
            xrtValueRelease(pParsed);
            return fail("negative instance index");
        }
        xrtClearError();
    }
    xrtValueRelease(pAddresses);
    xrtValueRelease(pAttributes);
    xrtValueRelease(pOutput);
    xrtValueRelease(pState);
    puts("terraform_state C smoke: PASS");
    return 0;
}
