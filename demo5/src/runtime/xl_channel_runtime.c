#include "../common/xl_common.h"
#include "xlrt_generated.h"

#define XL_CHANNEL_KEY_QUEUE "queue"
#define XL_CHANNEL_KEY_CLOSED "closed"
#define XL_CHANNEL_KEY_CAPACITY "capacity"

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

static xvalue __xlChannelMakeRecvResult(int bOk, xvalue item)
{
	xvalue result;

	result = xvoCreateArray();
	if ( result == NULL ) {
		if ( item != NULL ) {
			xvoUnref(item);
		}
		return NULL;
	}

	(void)xvoArrayAppendValue(result, xvoCreateBool(bOk ? TRUE : FALSE), TRUE);
	if ( item != NULL ) {
		(void)xvoArrayAppendValue(result, item, TRUE);
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
	return value;
}

int xl_channel_send(xvalue value, xvalue item)
{
	xvalue queue;
	const int iCapacity = __xlChannelCapacity(value);

	if ( item == NULL ) {
		item = xvoCreateNull();
	}
	if ( (value == NULL) || (queue = __xlChannelGetQueue(value)) == NULL ) {
		if ( item != NULL ) {
			xvoUnref(item);
		}
		return 0;
	}
	if ( __xlChannelIsClosed(value) ) {
		xvoUnref(item);
		return 0;
	}
	if ( (iCapacity > 0) && ((int)xvoArrayItemCount(queue) >= iCapacity) ) {
		xvoUnref(item);
		return 0;
	}
	if ( !xvoArrayAppendValue(queue, item, TRUE) ) {
		xvoUnref(item);
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

		itemCopy = (item != NULL) ? xvoCopy(item) : xvoCreateNull();
		(void)xvoArrayRemove(queue, 0u, 1u);
		return __xlChannelMakeRecvResult(1, itemCopy);
	}

	return __xlChannelMakeRecvResult(0, NULL);
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
