


/**************** json pool editor / 内存块json的编辑接口 ****************/

/*
 * json_mem_node_t - the block memory node
 * @list: the list value, LJSON associates `list` to the `head` of json_mem_mgr_t
 *   or the `list` of brother json_mem_node_t
 * @size: the memory size
 * @ptr: the memory pointer
 * @cur: the current memory pointer
 * @description: LJSON can use the block memory to accelerate memory allocation and save memory space.
 */
/*
 * json_mem_node_t - 块内存节点
 * @list: 链表节点，LJSON将 `list` 关联到 `json_mem_mgr_t` 的 `head` 或兄弟 `json_mem_node_t` 的 `list`
 * @size: 内存大小
 * @ptr: 内存指针
 * @cur: 当前内存指针
 * @description: LJSON使用块内存来加速内存分配并节省内存空间，内存块只能统一申请统一释放，无法单独释放某个对象
 */
typedef struct {
    struct json_list list;
    size_t size;
    char *ptr;
    char *cur;
} json_mem_node_t;

/*
 * json_mem_mgr_t - the node to manage block memory node
 * @head: the list head, LJSON manages block memory nodes through the list head
 * @mem_size: the default memory size to allocate, its default value is JSON_POOL_MEM_SIZE_DEF(8192)
 * @cur_node: the current block memory node
 * @description: the manage node manages a group of block memory nodes.
 */
/*
 * json_mem_mgr_t - 管理块内存节点的结构
 * @head: 链表头，LJSON通过链表头管理块内存节点
 * @mem_size: 默认分配的内存大小，默认值为 `JSON_POOL_MEM_SIZE_DEF`(8192)
 * @cur_node: 当前块内存节点
 * @description: 该管理节点管理一组块内存节点
 */
typedef struct {
    struct json_list head;
    size_t mem_size;
    json_mem_node_t *cur_node;
} json_mem_mgr_t;

/*
 * json_mem_t - the head to manage all types of block memory
 * @valid: IN, is there already memory allocation available to speed up
 *   frequent parsing of small JSON files
 * @obj_mgr: the node to manage json_object
 * @key_mgr: the node to manage the value of key
 * @str_mgr: the node to manage the value of JSON_STRING object
 * @description: The reason for dividing into multiple management nodes is that
 * there is a memory address alignment requirement for json_object.
 */
/*
 * json_mem_t - 管理所有类型块内存的结构
 * @valid: IN, 是否已经有内存分配可用于加速频繁解析小型JSON文件
 * @obj_mgr: 管理 `json_object` 的节点
 * @key_mgr: 管理键值的节点
 * @str_mgr: 管理 `JSON_STRING` 对象值的节点
 * @description: 划分为多个管理节点的原因是 `json_object` 有内存地址对齐要求
 *   valid设为false时，JSON解析函数内部会重新初始化mem，所以此时mem一定不要有挂载内存节点
 */
typedef struct {
    bool valid;
    json_mem_mgr_t obj_mgr;
    json_mem_mgr_t key_mgr;
    json_mem_mgr_t str_mgr;
} json_mem_t;





/**************** configuration ****************/

/* Whether to use manual loop unfolding */
#ifndef JSON_MANUAL_LOOP_UNFOLD
#define JSON_MANUAL_LOOP_UNFOLD         1
#endif

/* Whether to allow C-like single-line comments and multi-line comments */
#ifndef JSON_PARSE_SKIP_COMMENT
#define JSON_PARSE_SKIP_COMMENT         0
#endif

/* Whether to allow comma in last element of array or object */
#ifndef JSON_PARSE_LAST_COMMA
#define JSON_PARSE_LAST_COMMA           1
#endif

/* Whether to allow empty key */
#ifndef JSON_PARSE_EMPTY_KEY
#define JSON_PARSE_EMPTY_KEY            0
#endif

/* Whether to allow special characters such as newline in the string */
#ifndef JSON_PARSE_SPECIAL_CHAR
#define JSON_PARSE_SPECIAL_CHAR         1
#endif

/* Whether to allow single quoted string/key and unquoted key */
#ifndef JSON_PARSE_SPECIAL_QUOTES
#define JSON_PARSE_SPECIAL_QUOTES       0
#endif

/* Whether to allow HEX number */
#ifndef JSON_PARSE_HEX_NUM
#define JSON_PARSE_HEX_NUM              1
#endif

/* Whether to allow special number such as starting with '.', '+', '0' */
#ifndef JSON_PARSE_SPECIAL_NUM
#define JSON_PARSE_SPECIAL_NUM          1
#endif

/* Whether to allow special double such as nan, inf, -inf */
#ifndef JSON_PARSE_SPECIAL_DOUBLE
#define JSON_PARSE_SPECIAL_DOUBLE       1
#endif

/* Whether to allow json starting with non-array and non-object */
#ifndef JSON_PARSE_SINGLE_VALUE
#define JSON_PARSE_SINGLE_VALUE         1
#endif

/* Whether to allow characters other than spaces after finishing parsing */
#ifndef JSON_PARSE_FINISHED_CHAR
#define JSON_PARSE_FINISHED_CHAR        0
#endif



/**************** debug ****************/

/* error print */
#define JSON_ERROR_PRINT_ENABLE         1

#if JSON_ERROR_PRINT_ENABLE
#define JsonErr(fmt, ...) do {                                      \
    printf("[JsonErr][%s:%d] ", __func__, __LINE__);                \
    printf(fmt, ##__VA_ARGS__);                                     \
} while(0)

#define JsonPareseErr(s)      do {                                  \
    if (parse_ptr->str) {                                           \
        char ptmp[32] = {0};                                        \
        strncpy(ptmp, parse_ptr->str + parse_ptr->offset, 31);      \
        JsonErr("====%s====\n%s\n", s, ptmp);                       \
    } else {                                                        \
        JsonErr("%s\n", s);                                         \
    }                                                               \
} while(0)

#else
#define JsonErr(fmt, ...)     do {} while(0)
#define JsonPareseErr(s)      do {} while(0)
#endif



/**************** gcc builtin ****************/

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED_ATTR                     __attribute__((unused))
#define FALLTHROUGH_ATTR                __attribute__((fallthrough))
#define likely(cond)                    __builtin_expect(!!(cond), 1)
#define unlikely(cond)                  __builtin_expect(!!(cond), 0)
#else
#define UNUSED_ATTR
#define FALLTHROUGH_ATTR
#define likely(cond)                    (cond)
#define unlikely(cond)                  (cond)
#endif

#if JSON_PARSE_SPECIAL_QUOTES
#define UNUSED_END_CH
#else
#define UNUSED_END_CH                   UNUSED_ATTR
#endif



/**************** definition ****************/

/* head apis */
#define json_malloc                     xrtMalloc
#define json_calloc                     xrtCalloc
#define json_realloc                    xrtRealloc
#define json_strdup                     strdup
#define json_free                       xrtFree

#define JSON_ITEM_NUM_PLUS_DEF          16
#define JSON_POOL_MEM_SIZE_DEF          8192

/* print choice size */
#define JSON_PRINT_UTF16_SUPPORT        0
#define JSON_PRINT_NUM_INIT_DEF         1024
#define JSON_PRINT_NUM_PLUS_DEF         64
#define JSON_PRINT_DEPTH_DEF            16
#define JSON_PRINT_SIZE_PLUS_DEF        8192
#define JSON_FORMAT_ITEM_SIZE_DEF       32
#define JSON_UNFORMAT_ITEM_SIZE_DEF     24

/* file parse choice size */
#define JSON_PARSE_ERROR_STR            "Z"
#define JSON_PARSE_READ_SIZE_DEF        8192
#define JSON_PARSE_NUM_DIV_DEF          8





typedef struct _json_parse_t {
    size_t size;
    size_t offset;
    bool reuse_flag;

    char *str;
    json_mem_t *mem;

    void (*skip_blank)(struct _json_parse_t *parse_ptr);
    int (*parse_string)(struct _json_parse_t *parse_ptr, char end_ch, char **ppstr,
        json_strinfo_t *pinfo, json_mem_mgr_t *mgr);
    int (*parse_value)(struct _json_parse_t *parse_ptr, json_object **root);

    json_sax_parser_t parser;
    json_sax_cb_t cb;
    json_sax_ret_t ret;
} json_parse_t;

#define IS_BLANK(c)      ((((c) + 0xdf) & 0xff) > 0xdf)
#define IS_DIGIT(c)      ((c) >= '0' && (c) <= '9')





/**************** json print apis ****************/

XXAPI json_strinfo_t xrtJsonGetStringInfo(const char *str, const json_strinfo_t *orig)
{
    json_strinfo_t info;
    int i = 0;

    if (orig) {
        info = *orig;
        info.escaped = 0;
        info.len = 0;
    } else {
        memset(&info, 0, sizeof(info));
    }

    if (str) {
        for (i = 0; str[i]; ++i) {
            switch (str[i]) {
            case '\"': case '\\': case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
                info.escaped = 1;
                break;
            default:
#if JSON_PRINT_UTF16_SUPPORT
                if ((unsigned char)str[i] < ' ')
                    info.escaped = 1;
#endif
                break;
            }
        }
        info.len = i;
    }

    return info;
}

typedef struct _json_print_t {
    char *ptr;
    char *cur;
    int (*realloc)(struct _json_print_t *print_ptr, size_t slen);
    size_t size;

    size_t plus_size;
    size_t item_size;
    int item_total;
    int item_count;
    bool format_flag;
} json_print_t;

#define GET_BUF_USED_SIZE(bp) ((bp)->cur - (bp)->ptr)
#define GET_BUF_FREE_SIZE(bp) ((bp)->size - ((bp)->cur - (bp)->ptr))

static inline char _is_escape_char(uint8_t val)
{
#define ESCAPE_UTF16_VAL        1
#if JSON_PRINT_UTF16_SUPPORT
#define PRINT_STR_CMP_VAL       0
#else
#define PRINT_STR_CMP_VAL       ESCAPE_UTF16_VAL
#endif

    static const char char_escape_lut[256] = {
        1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 98 , 116, 110, 118, 102, 114, 1  , 1  ,
        1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  , 1  ,
        0  , 0  , 34 , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 92 , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
        0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0
    };

    return char_escape_lut[val];
}

static int _print_str_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t used = GET_BUF_USED_SIZE(print_ptr);
    size_t len = used + slen + 1;

    while (print_ptr->item_total < print_ptr->item_count) {
        print_ptr->item_total += JSON_PRINT_NUM_PLUS_DEF;
        print_ptr->size += print_ptr->plus_size >> 2;
    }
    if (print_ptr->item_total - print_ptr->item_count > print_ptr->item_count) {
        print_ptr->size += print_ptr->size;
    } else {
        print_ptr->size += (uint64_t)print_ptr->size *
            (print_ptr->item_total - print_ptr->item_count) / print_ptr->item_count;
    }

    while (print_ptr->size < len)
        print_ptr->size += print_ptr->plus_size;

    char *new_str = (char *)json_realloc(print_ptr->ptr, print_ptr->size);
    if (likely(new_str)) {
        print_ptr->ptr = new_str;
        print_ptr->cur = print_ptr->ptr + used;
    } else {
        JsonErr("malloc failed!\n");
        json_free(print_ptr->ptr);
        print_ptr->ptr = NULL;
        print_ptr->cur = print_ptr->ptr;
        return -1;
    }

    return 0;
}

#define _PRINT_PTR_REALLOC(nz) do {                 \
    if (unlikely(GET_BUF_FREE_SIZE(print_ptr) < (nz)\
        && print_ptr->realloc(print_ptr, nz) < 0))  \
        goto err;                                   \
} while(0)

#define _PRINT_PTR_NUMBER(fname, num) do {          \
    _PRINT_PTR_REALLOC(64);                         \
    print_ptr->cur += fname(num, print_ptr->cur);   \
} while(0)

#define _PRINT_PTR_STRNCAT(str, slen) do {          \
    _PRINT_PTR_REALLOC((slen + 1));                 \
    memcpy(print_ptr->cur, str, slen);              \
    print_ptr->cur += slen;                         \
} while(0)

static inline int _print_addi_format(json_print_t *print_ptr, size_t depth)
{
    _PRINT_PTR_REALLOC((depth + 2));
    *print_ptr->cur++ = '\n';
    memset(print_ptr->cur, '\t', depth);
    print_ptr->cur += depth;

    return 0;
err:
    return -1;
}
#define _PRINT_ADDI_FORMAT(ptr, depth) do { if (unlikely(_print_addi_format(ptr, depth) < 0)) goto err; } while(0)

static inline int _json_print_string(json_print_t *print_ptr, const char *val, json_strinfo_t *info)
{
#define _JSON_PRINT_SEGMENT()   do {  \
    size = str - bak - 1;             \
    memcpy(cur, bak, size);           \
    cur += size;                      \
    bak = str;                        \
} while(0)


    char c = '\0', ch = '\0';
    size_t len = 0, size = 0, alloced = 0;
    const char *str = NULL, *bak = NULL, *end = NULL;
    char *cur = NULL;

    str = val;
    len = info->len;
    end = str + len;
    if (likely(!info->escaped)) {
        alloced = len + 3;
        _PRINT_PTR_REALLOC(alloced);
        cur = print_ptr->cur;
        *cur++ = '\"';
        memcpy(cur, str, len);
        cur += len;
        *cur++ = '\"';
        print_ptr->cur = cur;
        return 0;
    }

#if JSON_PRINT_UTF16_SUPPORT
    alloced = (len << 2) + (len << 1) + 3;
#else
    alloced = (len << 1) + 3;
#endif
    _PRINT_PTR_REALLOC(alloced);
    cur = print_ptr->cur;
    *cur++ = '\"';
    bak = str;

loop:
#if JSON_MANUAL_LOOP_UNFOLD
    while (end - str >= 8) {
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
    }
#endif
    while (end > str) {
        c = *str++; ch = _is_escape_char((uint8_t)c); if (unlikely(ch > PRINT_STR_CMP_VAL)) goto next;
    }
    goto end;

next:
    _JSON_PRINT_SEGMENT();
#if JSON_PRINT_UTF16_SUPPORT
    if (unlikely(ch == ESCAPE_UTF16_VAL)) {
        static const char hex_char_lut[] = {
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
        };
        unsigned char uc = c;
        memcpy(cur, "\\u00", 4);
        cur += 4;
        *cur++ = hex_char_lut[uc >> 4 & 0xf];
        *cur++ = hex_char_lut[uc & 0xf];
    }
    else
#endif
    {
        *cur++ = '\\';
        *cur++ = ch;
    }
    goto loop;

end:
    ++str;
    _JSON_PRINT_SEGMENT();
    *cur++ = '\"';
    print_ptr->cur = cur;
    return 0;

err:
    JsonErr("malloc failed!\n");
    return -1;
}
#define _JSON_PRINT_STRING(ptr, val, info) do { if (unlikely(_json_print_string(ptr, val, info) < 0)) goto err; } while(0)

static int _print_val_release(json_print_t *print_ptr, bool free_all_flag, size_t *length, json_print_ptr_t *ptr)
{
#define _clear_free_ptr(ptr)    do { if (ptr) json_free(ptr); ptr = NULL; } while(0)
    int ret = 0;
    size_t used = GET_BUF_USED_SIZE(print_ptr);

	if (free_all_flag) {
		_clear_free_ptr(print_ptr->ptr);
	} else {
		if (length)
			*length = print_ptr->cur - print_ptr->ptr;
		*print_ptr->cur = '\0';

		if (ptr) {
			ptr->size = print_ptr->size;
			ptr->p = print_ptr->ptr;
		} else {
			/* Reduce size, never fail */
			print_ptr->ptr = (char *)json_realloc(print_ptr->ptr, used + 1);
		}
	}

    return ret;
}

static int _print_val_init(json_print_t *print_ptr, json_print_choice_t *choice)
{
    print_ptr->format_flag = choice->format_flag;
    print_ptr->plus_size = choice->plus_size ? choice->plus_size : JSON_PRINT_SIZE_PLUS_DEF;

	size_t item_size = 0;
	size_t total_size = 0;

	print_ptr->realloc = _print_str_ptr_realloc;
	item_size = (choice->format_flag) ? JSON_FORMAT_ITEM_SIZE_DEF : JSON_UNFORMAT_ITEM_SIZE_DEF;
	if (choice->item_size > item_size)
		item_size = choice->item_size;

	total_size = print_ptr->item_total * item_size;
	if (total_size < JSON_PRINT_SIZE_PLUS_DEF)
		total_size = JSON_PRINT_SIZE_PLUS_DEF;
	print_ptr->size = total_size;

    if (choice->ptr && choice->ptr->p) {
        print_ptr->size = choice->ptr->size;
        print_ptr->ptr = choice->ptr->p;
        choice->ptr->p = NULL;
    } else {
        if ((print_ptr->ptr = (char *)json_malloc(print_ptr->size)) == NULL) {
            JsonErr("malloc failed!\n");
            goto err;
        }
    }
    print_ptr->cur = print_ptr->ptr;

    return 0;
err:
    _print_val_release(print_ptr, true, NULL, NULL);
    return -1;
}





typedef struct {
    json_type_t type;
    int num;
} json_sax_print_depth_t;

typedef struct {
    int total;
    int count;
    json_sax_print_depth_t *array;

    json_print_t print_val;
    json_type_t last_type;
    bool error_flag;
} json_sax_print_t;

XXAPI int xrtJsonPrintValue(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value)
{
    json_sax_print_t *print_handle = (json_sax_print_t *)handle;
    json_print_t *print_ptr = &print_handle->print_val;
    json_string_t *jstr = NULL;
    int cur_pos = print_handle->count - 1;

    if (unlikely(print_handle->error_flag)) {
        return -1;
    }

    if (likely(print_handle->count > 0
        && !((type == JSON_ARRAY || type == JSON_OBJECT) && (*(json_sax_cmd_t*)value) == JSON_SAX_FINISH))) {
        // add ","
        if (print_handle->array[cur_pos].num > 0)
            _PRINT_PTR_STRNCAT(",", 1);

        // add key
        if (print_handle->array[cur_pos].type == JSON_OBJECT) {
            if (print_ptr->format_flag) {
                if (unlikely(!jkey || !jkey->str || !jkey->str[0])) {
#if !JSON_PARSE_EMPTY_KEY
                    JsonErr("key is empty!\n");
                    goto err;
#else
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                    _PRINT_PTR_STRNCAT("\"\":\t", 4);
#endif
                } else {
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                    xrtJsonUpdateStringInfo(jkey);
                    _JSON_PRINT_STRING(print_ptr, jkey->str, &jkey->info);
                    _PRINT_PTR_STRNCAT(":\t", 2);
                }
            } else {
                if (unlikely(!jkey || !jkey->str || !jkey->str[0])) {
#if !JSON_PARSE_EMPTY_KEY
                    JsonErr("key is empty!\n");
                    goto err;
#else
                    _PRINT_PTR_STRNCAT("\"\":", 3);
#endif
                } else {
                    xrtJsonUpdateStringInfo(jkey);
                    _JSON_PRINT_STRING(print_ptr, jkey->str, &jkey->info);
                    _PRINT_PTR_STRNCAT(":", 1);
                }
            }
        } else {
            if (print_ptr->format_flag) {
                if (type == JSON_ARRAY) {
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                } else {
                    if (print_handle->array[cur_pos].num > 0) {
                        _PRINT_PTR_STRNCAT(" ", 1);
                    } else {
                        if (type == JSON_OBJECT)
                            _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                    }
                }
            }
        }
    }

    // add value
    switch (type) {
    case JSON_NULL:
        _PRINT_PTR_STRNCAT("null", 4);
        break;
    case JSON_BOOL:
        if ((*(bool*)value)) {
            _PRINT_PTR_STRNCAT("true", 4);
        } else {
            _PRINT_PTR_STRNCAT("false", 5);
        }
        break;

    case JSON_INT:
        _PRINT_PTR_NUMBER(xrtI32ToStr, *(int32_t*)value);
        break;
    case JSON_HEX:
        _PRINT_PTR_NUMBER(xrtU32ToStr, *(uint32_t*)value);
        break;
    case JSON_LINT:
        _PRINT_PTR_NUMBER(xrtI64ToStr, *(int64_t*)value);
        break;
    case JSON_LHEX:
        _PRINT_PTR_NUMBER(xrtU64ToStr, *(uint64_t*)value);
        break;
    case JSON_DOUBLE:
        _PRINT_PTR_NUMBER(xrtNumToStr, *(double*)value);
        break;
    case JSON_STRING:
        jstr = (json_string_t*)value;
        if (unlikely(!jstr || !jstr->str || !jstr->str[0])) {
            _PRINT_PTR_STRNCAT("\"\"", 2);
        } else {
            xrtJsonUpdateStringInfo(jstr);
            _JSON_PRINT_STRING(print_ptr, jstr->str, &jstr->info);
        }
        break;

    case JSON_ARRAY:
    case JSON_OBJECT:
        switch ((*(json_sax_cmd_t*)value)) {
        case JSON_SAX_START:
            if (unlikely(print_handle->count == print_handle->total)) {
                print_handle->total += JSON_PRINT_DEPTH_DEF;
                json_sax_print_depth_t *new_array = (json_sax_print_depth_t *)json_realloc(
                    print_handle->array, print_handle->total * sizeof(json_sax_print_depth_t));
                if (new_array) {
                    print_handle->array = new_array;
                } else {
                    JsonErr("malloc failed!\n");
                    json_free(print_handle->array);
                    print_handle->array = NULL;
                    goto err;
                }
            }
            if (type == JSON_OBJECT) {
                _PRINT_PTR_STRNCAT("{", 1);
            } else {
                _PRINT_PTR_STRNCAT("[", 1);
            }
            if (print_handle->count > 0)
                ++print_handle->array[cur_pos].num;
            print_handle->array[print_handle->count].type = type;
            print_handle->array[print_handle->count].num = 0;
            ++print_handle->count;
            break;

        case JSON_SAX_FINISH:
            if (unlikely(print_handle->count == 0 || print_handle->array[cur_pos].type != type)) {
                JsonErr("unexpected array or object finish!\n");
                goto err;
            }
            if (print_ptr->format_flag) {
                if (print_handle->count > 1) {
                    if (print_handle->array[print_handle->count-1].num > 0) {
                        if (type == JSON_OBJECT) {
                            _PRINT_ADDI_FORMAT(print_ptr, cur_pos);
                        } else {
                            if (print_handle->last_type == JSON_OBJECT || print_handle->last_type == JSON_ARRAY)
                                _PRINT_ADDI_FORMAT(print_ptr, cur_pos);
                        }
                    }
                } else {
                    _PRINT_PTR_STRNCAT("\n", 1);
                }
            }
            if (type == JSON_OBJECT) {
                _PRINT_PTR_STRNCAT("}", 1);
            } else {
                _PRINT_PTR_STRNCAT("]", 1);
            }
            --print_handle->count;
            print_handle->last_type = type;
            return 0;

        default:
            goto err;
        }
        break;

    default:
        goto err;
    }

    print_handle->last_type = type;
    if (cur_pos >= 0)
        ++print_handle->array[cur_pos].num;
    ++print_ptr->item_count;

    return 0;
err:
    print_handle->error_flag = true;
    return -1;
}

XXAPI json_sax_print_hd xrtJsonPrintStart(json_print_choice_t *choice)
{
    json_sax_print_t *print_handle = NULL;

    if ((print_handle = (json_sax_print_t *)json_calloc(1, sizeof(json_sax_print_t))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    print_handle->print_val.item_total = choice->item_total ? choice->item_total : JSON_PRINT_NUM_INIT_DEF;
    if (_print_val_init(&print_handle->print_val, choice) < 0) {
        json_free(print_handle);
        return NULL;
    }

    print_handle->total = JSON_PRINT_DEPTH_DEF;
    if ((print_handle->array = (json_sax_print_depth_t *)json_malloc(print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
        _print_val_release(&print_handle->print_val, true, NULL, NULL);
        json_free(print_handle);
        JsonErr("malloc failed!\n");
        return NULL;
    }

    return print_handle;
}

XXAPI char* xrtJsonPrintFinish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr)
{
    char *ret = NULL;

    json_sax_print_t *print_handle = (json_sax_print_t *)handle;
    if (!print_handle)
        return NULL;
    if (print_handle->array)
        json_free(print_handle->array);
    if (print_handle->error_flag) {
        _print_val_release(&print_handle->print_val, true, NULL, NULL);
        json_free(print_handle);
        return NULL;
    }

    if (_print_val_release(&print_handle->print_val, false, length, ptr) < 0) {
        json_free(print_handle);
        return NULL;
    }
    // Get pointer after _print_val_release, because realloc may change the address
    ret = print_handle->print_val.ptr;
    json_free(print_handle);

    return ret;
}





static inline int _get_str_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size UNUSED_ATTR, char **sstr)
{
    size_t offset = parse_ptr->offset + read_offset;
    *sstr = parse_ptr->str + offset;
    return (int)(parse_ptr->size - offset);
}

static inline int _get_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    return _get_str_parse_ptr(parse_ptr, read_offset, read_size, sstr);
}
#define _UPDATE_PARSE_OFFSET(add_offset)    parse_ptr->offset += add_offset

static inline json_type_t _json_parse_number(const char **sstr, json_number_t *vnum)
{
    const char *s = *sstr;

#if !JSON_PARSE_HEX_NUM
    if (unlikely(*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X'))) {
        JsonErr("HEX can't be parsed in standard json!\n");
        return JSON_NULL;
    }
#endif

    json_type_t type;
    *sstr += xrtParseNum(s, (jnum_type_t *)&type, (jnum_value_t *)(vnum));
    return type;
}

static inline uint32_t _parse_hex4(const unsigned char *str)
{
    int i = 0;
    uint32_t val = 0;

    for (i = 0; i < 4; ++i) {
        switch (str[i]) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            val = (val << 4) + (str[i] - '0');
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            val = (val << 4) + 10 + (str[i] - 'a');
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            val = (val << 4) + 10 + (str[i] - 'A');
            break;
        default:
            return 0;
        }
    }

    return val;
}

static int utf16_literal_to_utf8(const unsigned char *start_str, const unsigned char *finish_str, unsigned char **pptr)
{ /* copy from cJSON */
    const unsigned char *str = start_str;
    unsigned char *ptr = *pptr;

    int seq_len = 0;
    int i = 0;
    unsigned long uc = 0;
    unsigned int  uc1 = 0, uc2 = 0;
    unsigned char len = 0;
    unsigned char first_byte_mark = 0;

    /* converts a UTF-16 literal to UTF-8, A literal can be one or two sequences of the form \uXXXX */
    if ((finish_str - str) < 6)                             /* input ends unexpectedly */
        goto fail;
    str += 2;
    uc1 = _parse_hex4(str);                                 /* get the first utf16 sequence */
    if (((uc1 >= 0xDC00) && (uc1 <= 0xDFFF)))               /* check first_code is valid */
        goto fail;
    if ((uc1 >= 0xD800) && (uc1 <= 0xDBFF)) {               /* UTF16 surrogate pair */
        str += 4;
        seq_len = 12;                                       /* \uXXXX\uXXXX */
        if ((finish_str - str) < 6)                         /* input ends unexpectedly */
            goto fail;
        if ((str[0] != '\\') || (str[1] != 'u'))            /* missing second half of the surrogate pair */
            goto fail;
        str += 2;
        uc2 = _parse_hex4(str);                             /* get the second utf16 sequence */
        if ((uc2 < 0xDC00) || (uc2 > 0xDFFF))               /* check second_code is valid */
            goto fail;
        uc = 0x10000 + (((uc1 & 0x3FF) << 10) | (uc2 & 0x3FF)); /* calculate the unicode uc from the surrogate pair */
    } else {
        seq_len = 6;                                        /* \uXXXX */
        uc = uc1;
    }

    /* encode as UTF-8 takes at maximum 4 bytes to encode: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (uc < 0x80) {                                        /* normal ascii, encoding 0xxxxxxx */
        len = 1;
    } else if (uc < 0x800) {                                /* two bytes, encoding 110xxxxx 10xxxxxx */
        len = 2;
        first_byte_mark = 0xC0;                             /* 11000000 */
    } else if (uc < 0x10000) {                              /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        len = 3;
        first_byte_mark = 0xE0;                             /* 11100000 */
    } else if (uc <= 0x10FFFF) {                            /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        len = 4;
        first_byte_mark = 0xF0;                             /* 11110000 */
    }  else {                                               /* invalid unicode */
        goto fail;
    }
    for (i = len - 1; i > 0; --i) {                         /* encode as utf8 */
        ptr[i] = (unsigned char)((uc | 0x80) & 0xBF);       /* 10xxxxxx */
        uc >>= 6;
    }
    if (len > 1) {                                          /* encode first byte */
        ptr[0] = (unsigned char)((uc | first_byte_mark) & 0xFF);
    } else {
        ptr[0] = (unsigned char)(uc & 0x7F);
    }

    *pptr += len;
    return seq_len;

fail:
    return 0;
}

static int _json_parse_key(json_parse_t *parse_ptr, json_string_t *jkey)
{
    char *str = NULL;
    char end_ch = '\0';

    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 2, &str);

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        if (unlikely(str[1] == str[0])) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(2);
#endif
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &jkey->str, &jkey->info,
                &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (unlikely(*str != ':')) {
            JsonPareseErr("key is not before ':'");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(1);
        break;

    default:
#if JSON_PARSE_SPECIAL_QUOTES
        if (unlikely(*str == ':')) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(1);
#endif
        } else {
            end_ch = ':';
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &jkey->str, &jkey->info,
                &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);

            while (IS_BLANK((unsigned char)jkey->str[jkey->info.len - 1]))
                --jkey->info.len;
            jkey->str[jkey->info.len] = '\0';
        }
        break;
#else
        JsonPareseErr("key is not started with quotes!");
        goto err;
#endif
    }
    return 0;
err:
    return -1;
}

static int _json_parse_single_value(json_parse_t *parse_ptr, char *str, json_strinfo_t *kinfo,
    json_number_t *pnum, char **ppstr, json_strinfo_t *pinfo)
{
    char end_ch = '\0';
    char *bak = NULL;

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        kinfo->type = JSON_STRING;
        if (unlikely(str[1] == str[0])) {
            *ppstr = NULL;
            memset(pinfo, 0, sizeof(*pinfo));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, ppstr, pinfo,
                &parse_ptr->mem->str_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

#if JSON_PARSE_SPECIAL_NUM
    case '+':
#endif
    case '-':
#if JSON_PARSE_SPECIAL_DOUBLE
        if (strncmp("inf", str + 1, 3) == 0) {
            kinfo->type = JSON_DOUBLE;
            pnum->vlhex = (*str == '-' ? 0xFFF0000000000000 : 0x7FF0000000000000);
            _UPDATE_PARSE_OFFSET(4);
            break;
        }
        FALLTHROUGH_ATTR;
#endif
#if JSON_PARSE_SPECIAL_NUM
    case '.':
#endif
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((kinfo->type = _json_parse_number((const char **)&str, pnum)) == JSON_NULL)) {
            JsonPareseErr("Not number!");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(str - bak);
        break;

    case 'f':
        if (likely(parse_ptr->size - parse_ptr->offset >= 5 && memcmp("false", str, 5) == 0)) {
            kinfo->type = JSON_BOOL;
            pnum->vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("true", str, 4) == 0)) {
            kinfo->type = JSON_BOOL;
            pnum->vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("null", str, 4) == 0)) {
            kinfo->type = JSON_NULL;
            _UPDATE_PARSE_OFFSET(4);
            break;
        }
#if JSON_PARSE_SPECIAL_DOUBLE
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("nan", str, 3) == 0)) {
            kinfo->type = JSON_DOUBLE;
            pnum->vlhex = 0x7FF0000000000001;
            _UPDATE_PARSE_OFFSET(3);
            break;
        }
#endif
        JsonPareseErr("invalid next ptr!");
        goto err;

#if JSON_PARSE_SPECIAL_DOUBLE
    case 'i':
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("inf", str, 3) == 0)) {
            kinfo->type = JSON_DOUBLE;
            pnum->vlhex = 0x7FF0000000000000;
            _UPDATE_PARSE_OFFSET(3);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
#endif
    default:
        JsonPareseErr("invalid next ptr!");
        goto err;
    }

    return 0;
err:
    return -1;
}

#if JSON_PARSE_SKIP_COMMENT
static bool _skip_comment_rapid(json_parse_t *parse_ptr, int *pcnt)
{
    char *str = parse_ptr->str + parse_ptr->offset + *pcnt;
    int cnt = 0;

    switch (*(str + 1)) {
    case '/':
        str += 2;
        *pcnt += 2;
        while (1) {
            switch (*str) {
            case '\n':
                ++str;
                ++*pcnt;
                goto next;
            case '\0':
                goto end;
            default:
                ++str;
                ++*pcnt;
                break;
            }
        }
        break;
    case '*':
        cnt = 2;
        str += 2;
        while (1) {
            switch (*str) {
            case '*':
                if (*(str + 1) == '/') {
                    str += 2;
                    cnt += 2;
                    *pcnt += cnt;
                    goto next;
                } else {
                    ++str;
                    ++cnt;
                }
                break;
            case '\0':
                goto end;
            default:
                ++str;
                ++cnt;
                break;
            }
        }
        break;
    default:
        goto end;
    }
next:
    return false;
end:
    return true;

}
#endif

static inline void _skip_blank_rapid(json_parse_t *parse_ptr)
{
    unsigned char *str, *bak;

#if JSON_PARSE_SKIP_COMMENT
next:
#endif
    str = (unsigned char *)(parse_ptr->str + parse_ptr->offset);
    bak = str;
    while (1) {
        if (IS_BLANK(*str)) ++str; else break;
#if JSON_MANUAL_LOOP_UNFOLD
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
        if (IS_BLANK(*str)) ++str; else break;
#endif
    }

    _UPDATE_PARSE_OFFSET(str - bak);

#if JSON_PARSE_SKIP_COMMENT
    if (unlikely(*str == '/')) {
        int cnt = 0;
        if (!_skip_comment_rapid(parse_ptr, &cnt)) {
            _UPDATE_PARSE_OFFSET(cnt);
            goto next;
        }
        _UPDATE_PARSE_OFFSET(cnt);
    }
#endif
}

static int _parse_strcpy(char *ptr, const char *str, int nsize)
{
    const char *bak = ptr, *last = str, *end = str + nsize;
    int size = 0, seq_len = 0;

    while (str < end) {
        if (unlikely(*str++ == '\\')) {
            size = (int)(str - last - 1);
            memcpy(ptr, last, size);
            ptr += size;

            switch ((*str++)) {
            case 'b' : *ptr++ = '\b'; break;
            case 'f' : *ptr++ = '\f'; break;
            case 'n' : *ptr++ = '\n'; break;
            case 'r' : *ptr++ = '\r'; break;
            case 't' : *ptr++ = '\t'; break;
            case 'v' : *ptr++ = '\v'; break;
            case '\"': *ptr++ = '\"'; break;
            case '\'': *ptr++ = '\''; break;
            case '\\': *ptr++ = '\\'; break;
            case '/' : *ptr++ = '/' ; break;
#if JSON_PARSE_SPECIAL_CHAR
            case '\r': if (*str == '\n') ++str; break;
            case '\n': break;
#endif
            case 'u' :
                str -= 2;
                if (unlikely((seq_len = utf16_literal_to_utf8((unsigned char*)str,
                    (unsigned char*)end, (unsigned char**)&ptr)) == 0)) {
                    JsonErr("invalid utf16 code(\\u%c)!\n", str[2]);
                    return -1;
                }
                str += seq_len;
                break;
            default :
                JsonErr("invalid escape character(\\%c)!\n", str[1]);
                return -1;
            }

            last = str;
        }
    }

    size = (int)(str - last);
    memcpy(ptr, last, size);
    ptr += size;
    *ptr = '\0';
    return (int)(ptr - bak);
}

static int _parse_strlen(json_parse_t *parse_ptr, char end_ch UNUSED_END_CH, int *escaped)
{
#define PARSE_READ_SIZE    128
    char *str = NULL, *bak = NULL;
    int total = 0, rsize = 0;
    char c = '\0';

    _get_parse_ptr(parse_ptr, 0, PARSE_READ_SIZE, &str);
    bak = str;

    while (1) {
        switch ((c = *str++)) {
        case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
        case '\'':
        case ':':
            if (c == end_ch)
#endif
            {
                total += (int)(str - bak - 1);
                return total;
            }
#if JSON_PARSE_SPECIAL_QUOTES
            if (c == '\"' && !*escaped)
                *escaped = -1;
            break;
#endif
        case '\\':
            if (likely(rsize != (str - bak))) {
                ++str;
                *escaped = 1;
            } else {
                --str;
                total += (int)(str - bak);
                if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 2)) {
                    JsonErr("last char is slash!\n");
                    goto err;
                }
                bak = str;
            }
            break;
        case '\0':
            --str;
            total += (int)(str - bak);
            if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 1)) {
                JsonErr("No more string!\n");
                goto err;
            }
            bak = str;
            break;
#if !JSON_PARSE_SPECIAL_CHAR
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in standard json!\n");
            goto err;
#endif
        default:
            break;
        }
    }

err:
    JsonPareseErr("str format err!");
    return -1;
}





static json_mem_t s_invalid_json_mem;





static inline int _json_sax_parse_string(json_parse_t *parse_ptr, char end_ch, char **ppstr,
    json_strinfo_t *pinfo, json_mem_mgr_t *mgr)
{
    char *ptr = NULL, *str = NULL;
    int len = 0, total = 0;
    int escaped = 0;

    *ppstr = NULL;
    memset(pinfo, 0, sizeof(*pinfo));

    if (unlikely((total = _parse_strlen(parse_ptr, end_ch, &escaped)) < 0)) {
        return -1;
    }
    len = total;
    _get_parse_ptr(parse_ptr, 0, total, &str);

    if (likely(escaped != 1)) {
        if (!(mgr == &parse_ptr->mem->key_mgr)) {
            pinfo->escaped = escaped != 0;
            pinfo->alloced = 0;
            pinfo->len = len;
            *ppstr = str;
        } else {
            if (unlikely((ptr = (char *)json_malloc(len+1)) == NULL)) {
                JsonErr("malloc failed!\n");
                return -1;
            }
            memcpy(ptr, str, len);
            ptr[len] = '\0';

            pinfo->escaped = escaped != 0;
            pinfo->alloced = 1;
            pinfo->len = len;
            *ppstr = ptr;
        }
    } else {
        if (unlikely((ptr = (char *)json_malloc(len+1)) == NULL)) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        if (unlikely((len = _parse_strcpy(ptr, str, len)) < 0)) {
            JsonErr("_parse_strcpy failed!\n");
            json_free(ptr);
            goto err;
        }

        pinfo->escaped = 1;
        pinfo->alloced = 1;
        pinfo->len = len;
        *ppstr = ptr;
    }
    _UPDATE_PARSE_OFFSET(total);

    return len;
err:
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_sax_parse_value(json_parse_t *parse_ptr)
{
    char *str = NULL;
    json_string_t *jkey = NULL, *parent = NULL;
    json_sax_value_t *value = &parse_ptr->parser.value;
    json_string_t *tarray = NULL;
    char end_ch = '\0';
    int i = 0;

    memset(value, 0, sizeof(*value));
    parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
    if (unlikely((parse_ptr->parser.array = (json_string_t *)json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memset(parse_ptr->parser.array, 0, sizeof(json_string_t));
    goto next3;

next1:
    if (unlikely(parse_ptr->parser.index >= parse_ptr->parser.total - 1)) {
        parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
        if (unlikely((tarray = (json_string_t *)json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
            JsonErr("malloc failed!\n");
            goto err;
        }

        memcpy(tarray, parse_ptr->parser.array, sizeof(json_string_t) * (parse_ptr->parser.index + 1));
        json_free(parse_ptr->parser.array);
        parse_ptr->parser.array = tarray;
    }

    parent = parse_ptr->parser.array + parse_ptr->parser.index;
    ++parse_ptr->parser.index;
    memset(parse_ptr->parser.array + parse_ptr->parser.index, 0, sizeof(json_string_t));

next2:
    if (parent->info.type == JSON_ARRAY)
        goto next3;
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    if (unlikely(_json_parse_key(parse_ptr, jkey) < 0)) {
        goto err;
    }

next3:
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 128, &str);

    if ((*str == '{') || (*str == '[')) {
        jkey->info.type = (*str == '[') ? JSON_ARRAY : JSON_OBJECT;
        end_ch = (*str == '[') ? ']' : '}';
        value->vcmd = JSON_SAX_START;
        _UPDATE_PARSE_OFFSET(1);
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
            goto end;
        }

        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != end_ch)) {
            goto next1;
        } else {
            value->vcmd = JSON_SAX_FINISH;
            _UPDATE_PARSE_OFFSET(1);
        }
    } else {
        if (_json_parse_single_value(parse_ptr, str, &jkey->info, &value->vnum,
            &value->vstr.str, &value->vstr.info) < 0)
            goto err;
    }

    parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
    if (jkey->info.type == JSON_STRING && value->vstr.info.alloced) {
        json_free(value->vstr.str);
    }
    memset(value, 0, sizeof(*value));
    if (jkey->info.alloced) {
        json_free(jkey->str);
    }
    memset(jkey, 0, sizeof(*jkey));
    if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
        --parse_ptr->parser.index;
        goto end;
    }

next4:
    if (likely(parse_ptr->parser.index > 0)) {
        /* parse_ptr->parser.index > 0, parent is definitely not NULL. */
        end_ch = (parent->info.type == JSON_ARRAY) ? ']' : '}';
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);

        if (likely(*str == ',')) {
            _UPDATE_PARSE_OFFSET(1);
#if !JSON_PARSE_LAST_COMMA
            goto next2;
#else
            parse_ptr->skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != end_ch)
                goto next2;
#endif
        }
        if (likely(*str == end_ch)) {
            _UPDATE_PARSE_OFFSET(1);
            --parse_ptr->parser.index;
            jkey = parse_ptr->parser.array + parse_ptr->parser.index;
            value->vcmd = JSON_SAX_FINISH;
            parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
            memset(value, 0, sizeof(*value));
            if (jkey->info.alloced) {
                json_free(jkey->str);
            }
            memset(jkey, 0, sizeof(*jkey));
            if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
                --parse_ptr->parser.index;
                goto end;
            }

            if (likely(parse_ptr->parser.index > 0)) {
                parent = parse_ptr->parser.array + parse_ptr->parser.index - 1;
                goto next4;
            }
        } else {
            JsonPareseErr("invalid object or array!");
            goto err;
        }
    }

    parse_ptr->parser.index = -1;
end:
    value->vcmd = JSON_SAX_FINISH;
    for (i =parse_ptr->parser.index; i >= 0; --i) {
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (parse_ptr->parser.array[i].info.alloced) {
            json_free(parse_ptr->parser.array[i].str);
        }
    }
    json_free(parse_ptr->parser.array);
    memset(&parse_ptr->parser, 0, sizeof(parse_ptr->parser));

    return 0;
err:
    if (parse_ptr->parser.array) {
        for (i = 0; i < parse_ptr->parser.index; ++i) {
            if (parse_ptr->parser.array[i].info.alloced) {
                json_free(parse_ptr->parser.array[i].str);
            }
        }
        json_free(parse_ptr->parser.array);
    }
    memset(&parse_ptr->parser, 0, sizeof(parse_ptr->parser));
    return -1;
}



/*
 * json_sax_parse_str - The SAX parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @cb: IN, the callback to process result passed by the SAX parser
 * @return: -1 on failure, 0 on success
 * description: LJSON directly parses the data from the string
 */
/*
 * json_sax_parse_str - 从字符串进行SAX解析
 * @text: IN, 要解析的字符串
 * @str_len: IN, 字符串的长度
 * @cb: IN, 处理SAX解析器传递结果的回调函数
 * @return: 失败返回-1；成功返回0
 * @description: LJSON直接从字符串解析数据
 */
XXAPI int xrtJsonParseSAX(str text, size_t str_len, json_sax_cb_t cb)
{
    int ret = -1;
    json_parse_t parse_val = {0};
	
    parse_val.mem = &s_invalid_json_mem;
    
	parse_val.str = text;
	parse_val.size = str_len ? str_len : strlen(text);
	parse_val.skip_blank = _skip_blank_rapid;
    
    parse_val.parse_string = _json_sax_parse_string;
    parse_val.cb = cb;
    parse_val.parser.userdata = NULL;

#if !JSON_PARSE_SINGLE_VALUE
    parse_val.skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset] != '{' && parse_val.str[parse_val.offset] != '[') {
        JsonErr("The first object isn't object or array!\n");
        goto end;
    }
#endif

    ret = _json_sax_parse_value(&parse_val);
#if !JSON_PARSE_FINISHED_CHAR
    if (ret == 0) {
        parse_val.skip_blank(&parse_val);
        if (parse_val.str[parse_val.offset]) {
            JsonErr("Extra trailing characters!\n%s\n", parse_val.str + parse_val.offset);
            ret = -1;
        }
    }
#endif

#if !JSON_PARSE_SINGLE_VALUE
end:
#endif

    return ret;
}











// 解析 JSON 文件到 Value

// JSON 解析上下文结构（线程安全）
typedef struct {
	xstack stack;
	xvalue root;
	xvalue cur;
} xrtJsonParseContext;

static json_sax_ret_t xvo_private_ParseJSON_Proc(json_sax_parser_t *parser)
{
	xrtJsonParseContext *ctx = (xrtJsonParseContext *)parser->userdata;
	json_string_t *jkey = &parser->array[parser->index];
	// 新结构数据入栈
	if ( ((jkey->info.type == JSON_ARRAY) || (jkey->info.type == JSON_OBJECT)) && (parser->value.vcmd == JSON_SAX_START) ) {
		if ( jkey->info.type == JSON_ARRAY ) {
			if ( ctx->root ) {
				if ( ctx->cur->Type == XVO_DT_ARRAY ) {
					xvalue arrNew = xvoCreateArray();
					xvoArrayAppendValue(ctx->cur, arrNew, TRUE);
					xrtStackPushPtr(ctx->stack, arrNew);
					ctx->cur = arrNew;
				} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
					xvalue arrNew = xvoCreateArray();
					char* sKey = xrtMalloc(jkey->info.len + 1);
					memcpy(sKey, jkey->str, jkey->info.len);
					sKey[jkey->info.len] = 0;
					xvoTableSetValue(ctx->cur, sKey, jkey->info.len, arrNew, TRUE);
					xrtStackPushPtr(ctx->stack, arrNew);
					ctx->cur = arrNew;
				}
			} else {
				ctx->root = xvoCreateArray();
				xrtStackPushPtr(ctx->stack, ctx->root);
				ctx->cur = ctx->root;
			}
		} else if ( jkey->info.type == JSON_OBJECT ) {
			if ( ctx->root ) {
				if ( ctx->cur->Type == XVO_DT_ARRAY ) {
					xvalue tblNew = xvoCreateTable();
					xvoArrayAppendValue(ctx->cur, tblNew, TRUE);
					xrtStackPushPtr(ctx->stack, tblNew);
					ctx->cur = tblNew;
				} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
					xvalue tblNew = xvoCreateTable();
					char* sKey = xrtMalloc(jkey->info.len + 1);
					memcpy(sKey, jkey->str, jkey->info.len);
					sKey[jkey->info.len] = 0;
					xvoTableSetValue(ctx->cur, sKey, jkey->info.len, tblNew, TRUE);
					xrtStackPushPtr(ctx->stack, tblNew);
					ctx->cur = tblNew;
				}
			} else {
				ctx->root = xvoCreateTable();
				xrtStackPushPtr(ctx->stack, ctx->root);
				ctx->cur = ctx->root;
			}
		}
    }
	if ( jkey->info.type == JSON_NULL ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendNull(ctx->cur);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetNull(ctx->cur, sKey, jkey->info.len);
			}
        } else {
			ctx->root = xvoCreateNull();
        }
	} else if ( jkey->info.type == JSON_BOOL ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendBool(ctx->cur, parser->value.vnum.vbool);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetBool(ctx->cur, sKey, jkey->info.len, parser->value.vnum.vbool);
			}
        } else {
			ctx->root = xvoCreateBool(parser->value.vnum.vbool);
        }
	} else if ( jkey->info.type == JSON_INT ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendInt(ctx->cur, parser->value.vnum.vint);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetInt(ctx->cur, sKey, jkey->info.len, parser->value.vnum.vint);
			}
        } else {
			ctx->root = xvoCreateInt(parser->value.vnum.vint);
        }
	} else if ( jkey->info.type == JSON_HEX ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendInt(ctx->cur, parser->value.vnum.vhex);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetInt(ctx->cur, sKey, jkey->info.len, parser->value.vnum.vhex);
			}
        } else {
			ctx->root = xvoCreateInt(parser->value.vnum.vhex);
        }
	} else if ( jkey->info.type == JSON_LINT ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendInt(ctx->cur, parser->value.vnum.vlint);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetInt(ctx->cur, sKey, jkey->info.len, parser->value.vnum.vlint);
			}
        } else {
			ctx->root = xvoCreateInt(parser->value.vnum.vlint);
        }
	} else if ( jkey->info.type == JSON_LHEX ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendInt(ctx->cur, parser->value.vnum.vlhex);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetInt(ctx->cur, sKey, jkey->info.len, parser->value.vnum.vlhex);
			}
        } else {
			ctx->root = xvoCreateInt(parser->value.vnum.vlhex);
        }
	} else if ( jkey->info.type == JSON_DOUBLE ) {
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendFloat(ctx->cur, parser->value.vnum.vdbl);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetFloat(ctx->cur, sKey, jkey->info.len, parser->value.vnum.vdbl);
			}
        } else {
			ctx->root = xvoCreateFloat(parser->value.vnum.vdbl);
        }
	} else if ( jkey->info.type == JSON_STRING ) {
		char* sText = xrtMalloc(parser->value.vstr.info.len + 1);
		memcpy(sText, parser->value.vstr.str, parser->value.vstr.info.len);
		sText[parser->value.vstr.info.len] = 0;
        if ( ctx->root ) {
			if ( ctx->cur->Type == XVO_DT_ARRAY ) {
				xvoArrayAppendText(ctx->cur, sText, parser->value.vstr.info.len, TRUE);
			} else if ( ctx->cur->Type == XVO_DT_TABLE ) {
				char* sKey = xrtMalloc(jkey->info.len + 1);
				memcpy(sKey, jkey->str, jkey->info.len);
				sKey[jkey->info.len] = 0;
				xvoTableSetText(ctx->cur, sKey, jkey->info.len, sText, parser->value.vstr.info.len, TRUE);
			}
        } else {
			ctx->root = xvoCreateText(sText, parser->value.vstr.info.len, TRUE);
        }
	}
    if ( ((jkey->info.type == JSON_ARRAY) || (jkey->info.type == JSON_OBJECT)) && (parser->value.vcmd == JSON_SAX_FINISH) ) {
		if ( ctx->stack > 0) {
			xrtStackPopPtr(ctx->stack);
			ctx->cur = xrtStackTopPtr(ctx->stack);
		}
	}
    return JSON_SAX_PARSE_CONTINUE;
}

// 内部解析函数（支持 userdata）
static int _xrt_json_parse_with_context(str text, size_t str_len, json_sax_cb_t cb, void *userdata)
{
    int ret = -1;
    json_parse_t parse_val = {0};
	
    parse_val.mem = &s_invalid_json_mem;
    parse_val.str = text;
    parse_val.size = str_len ? str_len : strlen(text);
    parse_val.skip_blank = _skip_blank_rapid;
    parse_val.parse_string = _json_sax_parse_string;
    parse_val.cb = cb;
    parse_val.parser.userdata = userdata;

#if !JSON_PARSE_SINGLE_VALUE
    parse_val.skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset] != '{' && parse_val.str[parse_val.offset] != '[') {
        JsonErr("The first object isn't object or array!\n");
        goto end;
    }
#endif

    ret = _json_sax_parse_value(&parse_val);
#if !JSON_PARSE_FINISHED_CHAR
    if (ret == 0) {
        parse_val.skip_blank(&parse_val);
        if (parse_val.str[parse_val.offset]) {
            JsonErr("Extra trailing characters!\n%s\n", parse_val.str + parse_val.offset);
            ret = -1;
        }
    }
#endif

#if !JSON_PARSE_SINGLE_VALUE
end:
#endif
    return ret;
}

XXAPI xvalue xrtParseJSON(str sText, size_t iSize)
{
	xrtJsonParseContext ctx = {0};
	ctx.stack = xrtStackCreate(256, sizeof(ptr));
	ctx.root = NULL;
	ctx.cur = NULL;
	if ( iSize == 0 ) {
		iSize = strlen(sText);
	}
	int iRet = _xrt_json_parse_with_context(sText, iSize, xvo_private_ParseJSON_Proc, &ctx);
	if ( iRet < 0 ) {
		xrtStackDestroy(ctx.stack);
		return xvoCreateNull();
	}
	xrtStackDestroy(ctx.stack);
	return ctx.root;
}
XXAPI xvalue xrtParseJSON_File(str sFile)
{
	xrtJsonParseContext ctx = {0};
	ctx.stack = xrtStackCreate(256, sizeof(ptr));
	ctx.root = NULL;
	ctx.cur = NULL;
	size_t iSize = 0;
	str sText = xrtFileGetAll(sFile, &iSize);
	if ( sText == NULL ) {
		xrtStackDestroy(ctx.stack);
		return xvoCreateNull();
	}
	int iRet = _xrt_json_parse_with_context(sText, iSize, xvo_private_ParseJSON_Proc, &ctx);
	xrtFree(sText);  // 释放文件内容
	if ( iRet < 0 ) {
		xrtStackDestroy(ctx.stack);
		return xvoCreateNull();
	}
	xrtStackDestroy(ctx.stack);
	return ctx.root;
}




// 将 xte Value 序列化为字符串
void xvo_private_Stringify_Table(json_sax_print_hd handle, xvalue varVal, json_string_t* sKey);
void xvo_private_Stringify_Array(json_sax_print_hd handle, xvalue varVal, json_string_t* sKey)
{
	xrtJsonPrintArray(handle, sKey, JSON_SAX_START);
	for ( int i = 0; i < varVal->vArray->Count; i++ ) {
		xvalue objItem = xvoArrayGetValue(varVal, i);
		if ( objItem->Type == XVO_DT_NULL ) {
			xrtJsonPrintNull(handle, NULL);
		} else if ( objItem->Type == XVO_DT_BOOL ) {
			xrtJsonPrintBool(handle, NULL, objItem->vInt);
		} else if ( objItem->Type == XVO_DT_INT ) {
			xrtJsonPrintInt64(handle, NULL, objItem->vInt);
		} else if ( objItem->Type == XVO_DT_FLOAT ) {
			xrtJsonPrintDouble(handle, NULL, objItem->vFloat);
		} else if ( objItem->Type == XVO_DT_TEXT ) {
			json_string_t jstr = {0};
			jstr.str = objItem->vText;
			xrtJsonUpdateStringInfo(&jstr);
			xrtJsonPrintString(handle, NULL, &jstr);
		} else if ( objItem->Type == XVO_DT_ARRAY ) {
			xvo_private_Stringify_Array(handle, objItem, NULL);
		} else if ( objItem->Type == XVO_DT_TABLE ) {
			xvo_private_Stringify_Table(handle, objItem, NULL);
		}
	}
	xrtJsonPrintArray(handle, NULL, JSON_SAX_FINISH);
}
int xvo_private_Stringify_Table_Proc(Dict_Key* pKey, xvalue* ppVal, json_sax_print_hd handle)
{
	xvalue objItem = *ppVal;
	json_string_t jkey = {0};
	jkey.str = pKey->Key;
	jkey.info.len = pKey->KeyLen;
	if ( objItem->Type == XVO_DT_NULL ) {
		xrtJsonPrintNull(handle, &jkey);
	} else if ( objItem->Type == XVO_DT_BOOL ) {
		xrtJsonPrintBool(handle, &jkey, objItem->vInt);
	} else if ( objItem->Type == XVO_DT_INT ) {
		xrtJsonPrintInt64(handle, &jkey, objItem->vInt);
	} else if ( objItem->Type == XVO_DT_FLOAT ) {
		xrtJsonPrintDouble(handle, &jkey, objItem->vFloat);
	} else if ( objItem->Type == XVO_DT_TEXT ) {
		json_string_t jstr = {0};
		jstr.str = objItem->vText;
		xrtJsonUpdateStringInfo(&jstr);
		xrtJsonPrintString(handle, &jkey, &jstr);
	} else if ( objItem->Type == XVO_DT_ARRAY ) {
		xvo_private_Stringify_Array(handle, objItem, &jkey);
	} else if ( objItem->Type == XVO_DT_TABLE ) {
		xvo_private_Stringify_Table(handle, objItem, &jkey);
	}
	return FALSE;
}
void xvo_private_Stringify_Table(json_sax_print_hd handle, xvalue varVal, json_string_t* sKey)
{
	xrtJsonPrintObject(handle, sKey, JSON_SAX_START);
	xrtDictWalk(varVal->vTable, (void*)xvo_private_Stringify_Table_Proc, (void*)handle);
	xrtJsonPrintObject(handle, NULL, JSON_SAX_FINISH);
}
XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize)
{
	// 初始化 SAX 字符串输出
	json_print_choice_t choice;
	memset(&choice, 0, sizeof(choice));
	choice.format_flag = bFormat;
    choice.item_total = 32;
    choice.ptr = NULL;
    json_sax_print_hd handle = xrtJsonPrintStart(&choice);
    // 遍历根节点
    if ( varVal->Type == XVO_DT_NULL ) {
		xrtJsonPrintNull(handle, NULL);
    } else if ( varVal->Type == XVO_DT_BOOL ) {
		xrtJsonPrintBool(handle, NULL, varVal->vInt);
    } else if ( varVal->Type == XVO_DT_INT ) {
		xrtJsonPrintInt64(handle, NULL, varVal->vInt);
    } else if ( varVal->Type == XVO_DT_FLOAT ) {
		xrtJsonPrintDouble(handle, NULL, varVal->vFloat);
    } else if ( varVal->Type == XVO_DT_TEXT ) {
		json_string_t jstr = {0};
		jstr.str = varVal->vText;
		xrtJsonUpdateStringInfo(&jstr);
		xrtJsonPrintString(handle, NULL, &jstr);
    } else if ( varVal->Type == XVO_DT_ARRAY ) {
		xvo_private_Stringify_Array(handle, varVal, NULL);
    } else if ( varVal->Type == XVO_DT_TABLE ) {
		xvo_private_Stringify_Table(handle, varVal, NULL);
    }
    // 返回结果
	return xrtJsonPrintFinish(handle, pRetSize, NULL);
}
XXAPI int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat)
{
	size_t iSize = 0;
	str sRet = xrtStringifyJSON(varVal, bFormat, &iSize);
	if ( sRet ) {
		xrtFilePutAll(sFile, sRet, iSize);
		return TRUE;
	} else {
		return FALSE;
	}
}