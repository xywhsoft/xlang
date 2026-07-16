#include "xrt.h"
#include "../third_party/libzip/lib/zipint.h"

ZIP_EXTERN bool zip_secure_random(zip_uint8_t* pBuffer, zip_uint16_t iLength)
{
	if ( pBuffer == NULL && iLength != 0u ) { return false; }
	xrtRandomBytes((uint8*)pBuffer, iLength);
	return true;
}

zip_uint32_t zip_random_uint32(void)
{
	zip_uint32_t iValue = 0u;
	xrtRandomBytes((uint8*)&iValue, sizeof(iValue));
	return iValue;
}
