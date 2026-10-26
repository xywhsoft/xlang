#include "xl6_crypto_aes_block_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoAesBlockEncryptBufferV1(void* self,void* buffer)
{xbytesview input;return xl6BufferInput(buffer,-1,&input)?xl6CryptoAesEncryptBlock(self,input):NULL;}
xvalue* xl6CryptoAesBlockDecryptBufferV1(void* self,void* buffer)
{xbytesview input;return xl6BufferInput(buffer,-1,&input)?xl6CryptoAesDecryptBlock(self,input):NULL;}
