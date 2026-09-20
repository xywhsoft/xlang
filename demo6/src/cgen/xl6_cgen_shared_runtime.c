#include "src/cgen/xl6_cgen_buffer.h"
#include "src/cgen/xl6_cgen_dir.h"
#include "src/cgen/xl6_cgen_process_run.h"
#include "src/cgen/xl6_cgen_file.h"
#include "src/cgen/xl6_cgen_http1.h"
#include "src/cgen/xl6_cgen_json_value.h"
#include "src/cgen/xl6_cgen_json_stream.h"
#include "src/cgen/xl6_cgen_serialization_callback.h"
#include "src/cgen/xl6_cgen_xson.h"
#include "src/cgen/xl6_cgen_net.h"
#include "src/cgen/xl6_cgen_text_codec.h"
#include "src/cgen/xl6_cgen_text_output.h"
#include "src/cgen/xl6_cgen_tls.h"
#include "src/cgen/xl6_cgen_websocket.h"

/* Generated-program runtime components shared by the legacy and XLIR C
 * backends.  Keep these emitters in an ordinary translation unit so either
 * backend can be linked and tested without pulling in the other backend's
 * unity implementation. */
#include "src/cgen/xl6_cgen_file.c"
#include "src/cgen/xl6_cgen_dir.c"
#include "src/cgen/xl6_cgen_process_run.c"
#include "src/cgen/xl6_cgen_buffer.c"
#include "src/cgen/xl6_cgen_text_codec.c"
#include "src/cgen/xl6_cgen_text_output.c"
#include "src/cgen/xl6_cgen_json_value.c"
#include "src/cgen/xl6_cgen_json_stream.c"
#include "src/cgen/xl6_cgen_serialization_callback.c"
#include "src/cgen/xl6_cgen_xson.c"
#include "src/cgen/xl6_cgen_http1.c"
#define XL6_CGEN_NET_TCP_COMPONENT_IMPLEMENTATION 1
#define XL6_CGEN_NET_BUFFER_COMPONENT_IMPLEMENTATION 1
#include "src/cgen/xl6_cgen_net_transport.c"
#undef XL6_CGEN_NET_BUFFER_COMPONENT_IMPLEMENTATION
#undef XL6_CGEN_NET_TCP_COMPONENT_IMPLEMENTATION
#include "src/cgen/xl6_cgen_net_datagram.c"
#include "src/cgen/xl6_cgen_tls.c"
#include "src/cgen/xl6_cgen_websocket.c"
