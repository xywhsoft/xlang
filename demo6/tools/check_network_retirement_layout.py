"""Fail if the retired network generators or implicit module service return."""
from pathlib import Path
import argparse


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    root=parser.parse_args().root.resolve()
    retired=('src/cgen/xl6_cgen_net.h','src/cgen/xl6_cgen_net_transport.c',
             'src/cgen/xl6_cgen_net_datagram.c','src/cgen/xl6_ir_cgen_emit_net.c',
             'src/cgen/xl6_ir_cgen_emit_net_datagram.c','src/ir/xl6_ir_net_datagram_bridge.c')
    for name in retired:
        assert not (root/name).exists(), name
    forbidden=('xl6_xrt_tcp_','xl6_xrt_udp_','xl6_xrt_net_',
               'ModuleNetworkService','networkServiceAbi','xlir_network_services',
               'needsNetLifecycleRuntime','needsNetTcpRuntime','needsNetDatagramRuntime',
               'xl6CgenEmitNetTcpRuntime','xl6IrCgenEmitNetTransportRuntime')
    files=[p for area in ('src/ir','src/cgen','src/stdlib') for p in (root/area).rglob('*')
           if p.suffix in ('.c','.h','.inc')]
    for path in files:
        text=path.read_text(encoding='utf-8-sig')
        for token in forbidden: assert token not in text,(path,token)
    print('network retirement layout:',len(retired),'generators absent;',len(files),'production files free of retired network inference')


if __name__=='__main__':
    main()
