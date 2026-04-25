#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XL_CHANNEL_KEY_QUEUE "queue"
#define XL_CHANNEL_KEY_CLOSED "closed"
#define XL_CHANNEL_KEY_CAPACITY "capacity"
#define XL_CHANNEL_KEY_MARKER "__xl_channel__"
static xvalue __xlChannelGetQueue(xvalue value)
{
	return xvoTableGetValue(value, XL_CHANNEL_KEY_QUEUE, (uint32)(sizeof(XL_CHANNEL_KEY_QUEUE) - 1));
}

static int __xlChannelIsClosed(xvalue value)
{
	return xvoTableGetInt(value, XL_CHANNEL_KEY_CLOSED, (uint32)(sizeof(XL_CHANNEL_KEY_CLOSED) - 1)) ? 1 : 0;
}

static int __xlChannelCapacity(xvalue value)
{
	return (int)xvoTableGetInt(value, XL_CHANNEL_KEY_CAPACITY, (uint32)(sizeof(XL_CHANNEL_KEY_CAPACITY) - 1));
}

int xl_channel_is_value(xvalue value)
{
	if ( (value == NULL) || (xvoType(value) != XVO_DT_TABLE) ) {
		return 0;
	}
	if ( __xlChannelGetQueue(value) == NULL ) {
		return 0;
	}
	return xvoTableGetInt(value, XL_CHANNEL_KEY_MARKER, (uint32)(sizeof(XL_CHANNEL_KEY_MARKER) - 1)) ? 1 : 0;
}

static xvalue __xlChannelCopyItem(xvalue item)
{
	return xl_value_copy_preserve_refs(item);
}

static void __xlChannelReleaseItem(xvalue item)
{
	if ( item == NULL ) {
		return;
	}
	if ( (item->Type == XVO_DT_CLASS) || (item->Type == XVO_DT_CUSTOM) ) {
		xl_object_release(item);
		return;
	}
	xvoUnref(item);
}

static xvalue __xlChannelMakeRecvResult(int bOk, xvalue item)
{
	xvalue result;

	result = xvoCreateArray();
	if ( result == NULL ) {
		__xlChannelReleaseItem(item);
		return NULL;
	}

	(void)xvoArrayAppendValue(result, xvoCreateBool(bOk ? TRUE : FALSE), TRUE);
	if ( item != NULL ) {
		if ( !xvoArrayAppendValue(result, item, TRUE) ) {
			__xlChannelReleaseItem(item);
			xvoUnref(result);
			return NULL;
		}
	} else {
		(void)xvoArrayAppendValue(result, xvoCreateNull(), TRUE);
	}
	return result;
}

xvalue xl_channel_create(int iCapacity)
{
	xvalue value;
	xvalue queue;

	if ( iCapacity < 0 ) {
		iCapacity = 0;
	}

	value = xvoCreateTable();
	if ( value == NULL ) {
		return NULL;
	}

	queue = xvoCreateArray();
	if ( queue == NULL ) {
		xvoUnref(value);
		return NULL;
	}

	(void)xvoTableSetValue(value, XL_CHANNEL_KEY_QUEUE, (uint32)(sizeof(XL_CHANNEL_KEY_QUEUE) - 1), queue, TRUE);
	(void)xvoTableSetValue(value, XL_CHANNEL_KEY_CLOSED, (uint32)(sizeof(XL_CHANNEL_KEY_CLOSED) - 1), xvoCreateBool(FALSE), TRUE);
	(void)xvoTableSetValue(value, XL_CHANNEL_KEY_CAPACITY, (uint32)(sizeof(XL_CHANNEL_KEY_CAPACITY) - 1), xvoCreateInt(iCapacity), TRUE);
	(void)xvoTableSetValue(value, XL_CHANNEL_KEY_MARKER, (uint32)(sizeof(XL_CHANNEL_KEY_MARKER) - 1), xvoCreateBool(TRUE), TRUE);
	return value;
}

int xl_channel_send(xvalue value, xvalue item)
{
	xvalue queue;
	xvalue itemCopy;
	const int iCapacity = __xlChannelCapacity(value);

	if ( (value == NULL) || (queue = __xlChannelGetQueue(value)) == NULL ) {
		return 0;
	}
	if ( __xlChannelIsClosed(value) ) {
		return 0;
	}
	if ( (iCapacity > 0) && ((int)xvoArrayItemCount(queue) >= iCapacity) ) {
		return 0;
	}
	itemCopy = __xlChannelCopyItem(item);
	if ( itemCopy == NULL ) {
		return 0;
	}
	if ( !xvoArrayAppendValue(queue, itemCopy, TRUE) ) {
		__xlChannelReleaseItem(itemCopy);
		return 0;
	}
	return 1;
}

xvalue xl_channel_recv(xvalue value)
{
	xvalue queue;
	xvalue itemCopy;

	if ( (value == NULL) || (queue = __xlChannelGetQueue(value)) == NULL ) {
		return __xlChannelMakeRecvResult(0, NULL);
	}
	if ( xvoArrayItemCount(queue) != 0 ) {
		xvalue item = xvoArrayGetValue(queue, 0u);

		itemCopy = __xlChannelCopyItem(item);
		(void)xvoArrayRemove(queue, 0u, 1u);
		return __xlChannelMakeRecvResult(1, itemCopy);
	}

	return __xlChannelMakeRecvResult(0, NULL);
}

int xl_channel_len(xvalue value)
{
	xvalue queue;

	if ( (value == NULL) || (queue = __xlChannelGetQueue(value)) == NULL ) {
		return 0;
	}
	return (int)xvoArrayItemCount(queue);
}

int xl_channel_capacity(xvalue value)
{
	if ( value == NULL ) {
		return 0;
	}
	return __xlChannelCapacity(value);
}

int xl_channel_closed(xvalue value)
{
	if ( value == NULL ) {
		return 0;
	}
	return __xlChannelIsClosed(value);
}

int xl_channel_close(xvalue value)
{
	if ( value == NULL ) {
		return 0;
	}
	if ( __xlChannelIsClosed(value) ) {
		return 0;
	}
	(void)xvoTableSetValue(
		value,
		XL_CHANNEL_KEY_CLOSED,
		(uint32)(sizeof(XL_CHANNEL_KEY_CLOSED) - 1),
		xvoCreateBool(TRUE),
		TRUE
	);
	return 1;
}
