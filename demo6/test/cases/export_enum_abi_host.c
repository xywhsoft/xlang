#include "../../lib/xrt/xrt.h"
#include "export_enum_abi.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, code) \
    do { \
        if ( !(condition) ) { \
            fprintf(stderr, "enum C ABI check failed: %d\n", (code)); \
            result = (code); \
            goto cleanup; \
        } \
    } while ( 0 )

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    const xrt_type_desc* type;
    const xrtenum* info;
    const xrtenumvariant* variant;
    const xrtenumfield* field;
    XL_Enum_Packet__int packet = {0};
    XL_Enum_Packet__int packetCopy = {0};
    XL_Enum_Packet__int packetMove = {0};
    XL_Enum_Packet__string failure = {0};
    XL_Enum_Packet__string failureCopy = {0};
    XL_Enum_Batch batch = {0};
    XL_Class_EnumInspector inspector = {0};
    xtarray values = NULL;
    int64_t item;
    int result = 0;

    if ( !xlInit(&options) ) {
        return 1;
    }

    /* 数值 enum 的 C 常量、底层类型和反射信息必须完全一致。 */
    CHECK(XL6_ENUM_XL_Enum_HttpStatus__ok == 200, 2);
    CHECK(XL6_ENUM_XL_Enum_HttpStatus__created == 201, 3);
    CHECK(XL6_ENUM_XL_Enum_HttpStatus__notFound == 404, 4);
    type = xl6_enum_type_XL_Enum_HttpStatus();
    info = xrtEnumInfo(type);
    CHECK(type != NULL && info != NULL, 5);
    CHECK((info->Flags & XRT_ENUM_FLAG_NUMERIC) != 0, 6);
    CHECK(info->VariantCount == 3, 7);
    CHECK(xrtTypeSame(info->BackingType, xrtTypeUInt16()), 8);
    variant = xrtEnumFindValue(info, 404);
    CHECK(variant != NULL && variant->NameSize == 8, 9);
    CHECK(memcmp(variant->Name, "notFound", 8) == 0, 10);

    /* tagged enum 的字段偏移必须能直接定位公开 C 结构体中的 payload。 */
    type = xl6_enum_type_XL_Enum_Packet__int();
    info = xrtEnumInfo(type);
    CHECK(type != NULL && info != NULL, 11);
    CHECK((info->Flags & XRT_ENUM_FLAG_TAGGED) != 0, 12);
    CHECK(info->VariantCount == 3, 13);
    variant = xrtEnumFindVariant(info, "failure", 0);
    CHECK(variant != NULL && variant->FieldCount == 2, 14);
    field = xrtEnumFindField(variant, "message", 0);
    CHECK(field != NULL, 15);
    CHECK(field->Offset ==
          offsetof(XL_Enum_Packet__int, payload.failure.message), 16);
    CHECK(xrtTypeSame(field->Type, xrtTypeString()), 17);

    /* 公开构造、复制、移动和销毁 API 都遵守独立所有权。 */
    CHECK(xl6_enum_make_XL_Enum_Packet__int__value(&packet, 42), 18);
    CHECK(packet.tag == XL6_ENUM_TAG_XL_Enum_Packet__int__value, 19);
    CHECK(packet.payload.value.item == 42, 20);
    CHECK(xl6_enum_copy_XL_Enum_Packet__int(&packetCopy, &packet), 21);
    CHECK(packetCopy.payload.value.item == 42, 22);
    CHECK(xl6_enum_move_XL_Enum_Packet__int(&packetMove, &packetCopy), 23);
    CHECK(packetMove.payload.value.item == 42 && packetCopy.tag == 0, 24);

    CHECK(xl6_enum_make_XL_Enum_Packet__string__failure(
              &failure,
              503,
              "service unavailable"), 25);
    CHECK(xl6_enum_copy_XL_Enum_Packet__string(&failureCopy, &failure), 26);
    CHECK(failure.payload.failure.message != NULL, 27);
    CHECK(failureCopy.payload.failure.message != NULL, 28);
    CHECK(strcmp(failureCopy.payload.failure.message,
                 "service unavailable") == 0, 29);
    CHECK(failureCopy.payload.failure.message !=
          failure.payload.failure.message, 30);

    /* 显式 C 包装器不得消费调用方传入的 enum。 */
    CHECK(enum_packet_value(packet) == 42, 31);
    CHECK(packet.tag == XL6_ENUM_TAG_XL_Enum_Packet__int__value, 32);
    CHECK(enum_failure_code(failure) == 503, 33);
    CHECK(failure.tag == XL6_ENUM_TAG_XL_Enum_Packet__string__failure, 34);

    /* 容器参数由 C 调用方拥有，包装器复制后再交给 xlang。 */
    values = xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
    CHECK(values != NULL, 35);
    item = 10;
    CHECK(xrtTypedArrayAppend(values, &item) != NULL, 36);
    item = 20;
    CHECK(xrtTypedArrayAppend(values, &item) != NULL, 37);
    item = 30;
    CHECK(xrtTypedArrayAppend(values, &item) != NULL, 38);
    batch = enum_make_batch(values);
    CHECK(xrtTypedArrayCount(values) == 3, 39);
    CHECK(enum_batch_count(batch) == 3, 40);
    CHECK(xrtTypedArrayCount(batch.payload.items.values) == 3, 41);
    CHECK(enum_inspector_packet_value(&inspector, packet) == 42, 42);
    CHECK(packet.tag == XL6_ENUM_TAG_XL_Enum_Packet__int__value, 43);
    CHECK(enum_inspector_batch_count(&inspector, values) == 3, 44);
    CHECK(xrtTypedArrayCount(values) == 3, 45);

    CHECK(enum_kind_int(9) == 10, 46);
    CHECK(enum_kind_string("你好") == 2, 47);

cleanup:
    xl6_enum_drop_XL_Enum_Batch(&batch);
    xrtTypedArrayDestroy(values);
    xl6_enum_drop_XL_Enum_Packet__string(&failureCopy);
    xl6_enum_drop_XL_Enum_Packet__string(&failure);
    xl6_enum_drop_XL_Enum_Packet__int(&packetMove);
    xl6_enum_drop_XL_Enum_Packet__int(&packetCopy);
    xl6_enum_drop_XL_Enum_Packet__int(&packet);
    xlUnit();
    return result;
}
