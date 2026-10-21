#define main existingTcpContractMain
#include "xlir_tcp_managed_contract.c"
#undef main
int main(int argc,char** argv)
{
    assert(argc==3);check(argv[1]);check(argv[2]);
    assert(arrays>=1&&policies>=2&&refusals>100);
    printf("TCP immutable batch: exact NetBytes class-array admission, status enum, checked nonblocking ABI; %zu refusals, %zu zero-output guards\n",refusals,emissionGuards);
    return 0;
}
