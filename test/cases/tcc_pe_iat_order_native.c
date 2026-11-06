/* Real GCC COFF dllimport slot, loaded before or after a TCC plain reference. */
__declspec(dllimport) void *TlsGetValue(unsigned long index);
void *tcc_iat_native(unsigned long index)
{
    return TlsGetValue(index);
}
