#ifndef XL6_IO_HANDLE_H
#define XL6_IO_HANDLE_H
/* Closed private factory provenance. Unknown/host streams are not certified.
 * contextOwner describes the ONE reference actually owned by a unique native
 * callback context; it is borrowed graph metadata, never an extra retain or a
 * fabricated reference-counted node. Move/close it with the native stream. */
enum { XL6_IO_ORIGIN_UNKNOWN=0, XL6_IO_ORIGIN_BUILTIN=1, XL6_IO_ORIGIN_PROCESS=2 };
typedef struct xlir_io_reader { xreader* stream; unsigned origin; xrtownershipref contextOwner; } xlir_io_reader;
typedef struct xlir_io_writer { xwriter* stream; void* owner; void (*drop)(void*); unsigned origin; xrtownershipref contextOwner; } xlir_io_writer;

static inline bool xl6IoContextTrace(unsigned origin,const void* stream,xrtownershipref owner,
    xrtownershipvisitor visit,void* context)
{
    if(!visit)return false;
    if(origin==XL6_IO_ORIGIN_BUILTIN || (origin==XL6_IO_ORIGIN_PROCESS && !stream))
        return !owner.Data && !owner.Ops;
    return origin==XL6_IO_ORIGIN_PROCESS && owner.Data && owner.Ops && visit(owner,context);
}
#endif
