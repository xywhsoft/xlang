static const char xl6HttpServerTypesSource[] =
    "#ifndef XL6_HTTP_SERVER_RUNTIME\n"
    "#define XL6_HTTP_SERVER_RUNTIME\n"
    "#include <stdbool.h>\n"
    "#include <stddef.h>\n"
    "#include <stdint.h>\n"
    "#include <limits.h>\n"
    "#include <string.h>\n\n"
    "#ifndef XL_RUNTIME_H\n"
    "#ifndef XL6_XRT_STRVIEW_TYPEDEF\n"
    "#define XL6_XRT_STRVIEW_TYPEDEF\n"
    "typedef struct xstrview { const char* Data; size_t Size; } xstrview;\n"
    "#endif\n"
    "#ifndef XL6_XRT_BYTESVIEW_TYPEDEF\n"
    "#define XL6_XRT_BYTESVIEW_TYPEDEF\n"
    "typedef struct xbytesview { const unsigned char* Data; size_t Size; } xbytesview;\n"
    "#endif\n"
    "#ifndef XL6_XRT_HTTP_FIELD_TYPEDEF\n"
    "#define XL6_XRT_HTTP_FIELD_TYPEDEF\n"
    "typedef struct xhttpfield { xstrview Name; xstrview Value; } xhttpfield;\n"
    "#endif\n"
    "typedef struct xhttpbody xhttpbody;\n"
    "#ifndef XL6_XRT_QUERY_PAIR_TYPEDEF\n"
    "#define XL6_XRT_QUERY_PAIR_TYPEDEF\n"
    "typedef struct xquerypair { uint32_t Flags; xstrview Key; xstrview Value; } xquerypair;\n"
    "#endif\n"
    "#ifndef XL6_XRT_FORM_DATA_PART_TYPEDEF\n"
    "#define XL6_XRT_FORM_DATA_PART_TYPEDEF\n"
    "typedef struct xformdatapart { xstrview Name; xstrview Filename; xstrview ContentType; xhttpbody* Body; uint64_t Length; uint32_t Flags; } xformdatapart;\n"
    "#endif\n"
    "#ifndef XL6_XRT_PEM_TYPES\n"
    "#define XL6_XRT_PEM_TYPES\n"
    "typedef struct xpemblock { xstrview Label; xstrview Body; xstrview Raw; } xpemblock;\n"
    "typedef struct xpemcursor { xstrview Text; size_t Offset; } xpemcursor;\n"
    "#endif\n"
    "#endif\n\n"
    "#ifndef XL_RUNTIME_H\n"
    XL6_HTTP_SERVER_SOURCE(
typedef struct xhttpserver xhttpserver;
typedef struct xhttpconn xhttpconn;
typedef struct xhttpserverrequest xhttpserverrequest;
typedef struct xhttpreply xhttpreply;
typedef struct xhttpserverrouter xhttpserverrouter;
typedef struct xhttpservermux xhttpservermux;
typedef struct xqueryparams xqueryparams;
typedef struct xformdata xformdata;
typedef struct xtaskpool xtaskpool;
typedef struct xtlsidentity xtlsidentity;
typedef struct xtlscontext xtlscontext;
typedef struct xtlsresume xtlsresume;
typedef struct xtlsstream xtlsstream;
typedef struct xtlssession xtlssession;
typedef struct xroot_impl* xroot;

typedef struct xurl {
    uint32_t Flags;
    uint16_t Port;
    xstrview Scheme;
    xstrview Authority;
    xstrview UserInfo;
    xstrview Host;
    xstrview PortText;
    xstrview Path;
    xstrview Query;
    xstrview Fragment;
} xurl;

typedef struct xhttptarget {
    int Form;
    xstrview Method;
    xstrview Text;
    xurl Uri;
} xhttptarget;

typedef struct xhttprouteparam {
    xstrview Name;
    xstrview Value;
} xhttprouteparam;

typedef struct xcookiepair {
    xstrview Name;
    xstrview Value;
} xcookiepair;

typedef struct xmediatype {
    xstrview Type;
    xstrview Subtype;
    xstrview Parameters;
} xmediatype;

typedef struct xcookieattribute {
    uint32_t Flags;
    xstrview Name;
    xstrview Value;
} xcookieattribute;

typedef struct xsetcookie {
    uint32_t Flags;
    xstrview Name;
    xstrview Value;
    xstrview Domain;
    xstrview Path;
    xstrview RawAttributes;
    int64_t Expires;
    int64_t MaxAge;
    int SameSite;
    int Priority;
    const xcookieattribute* Extensions;
    size_t ExtensionCount;
} xsetcookie;

typedef struct xmultipartlimits {
    size_t MaxParts;
    size_t MaxHeaders;
    size_t MaxDelimiterBytes;
    size_t MaxHeaderBytes;
    size_t MaxPartBytes;
    size_t MaxBodyBytes;
} xmultipartlimits;

typedef struct xmultiparterrorinfo {
    int Code;
    size_t Offset;
} xmultiparterrorinfo;

typedef struct xformdataconfig {
    size_t InitialParts;
    size_t MaxParts;
    size_t MaxName;
    size_t MaxFilename;
    size_t MaxContentType;
    size_t MaxMetadata;
    uint64_t MaxPartBytes;
    uint64_t MaxBodyBytes;
} xformdataconfig;

typedef struct xhttp1limits {
    size_t MaxHead;
    size_t MaxStartLine;
    size_t MaxFieldLine;
    size_t MaxFields;
} xhttp1limits;

typedef struct xhttp1bodylimits {
    uint64_t MaxBody;
    size_t MaxChunkLine;
    size_t MaxTrailer;
    size_t MaxTrailerLine;
    size_t MaxTrailers;
} xhttp1bodylimits;

typedef struct xhttp1serverconfig {
    xhttp1limits Head;
    xhttp1bodylimits Body;
    bool AllowRawTransferCodings;
} xhttp1serverconfig;

typedef struct xnetserverconfig {
    xnetlistenconfig Listen;
    const xnetlistenconfig* Additional;
    size_t AdditionalCount;
    uint32_t AcceptQueueLimit;
    int Mode;
    bool SharedPort;
} xnetserverconfig;

typedef struct xhttpserverconfig {
    xnetserverconfig Network;
    xhttp1serverconfig Http1;
    size_t WriteSize;
    uint64_t HeaderTimeout;
    uint64_t BodyTimeout;
    uint64_t RequestTimeout;
    uint64_t IdleTimeout;
    uint64_t WriteTimeout;
    size_t MaxConnections;
    size_t MaxInformations;
} xhttpserverconfig;

typedef struct xhttpserverstats {
    int State;
    uint64_t Accepted;
    uint64_t Rejected;
    uint64_t Requests;
    uint64_t Responses;
    uint64_t Informations;
    uint64_t Upgraded;
    uint64_t ProtocolErrors;
    uint64_t Timeouts;
    size_t Connections;
    size_t PeakConnections;
    size_t Endpoints;
    size_t Listeners;
    bool Secure;
} xhttpserverstats;

typedef struct xhttpserverrouteevents {
    int (*Headers)(xhttpserver*, xhttpconn*, const xhttpserverrequest*, const xhttprouteparam*, size_t, void*);
    bool (*Body)(xhttpserver*, xhttpconn*, const xhttpserverrequest*, const xhttprouteparam*, size_t, xbytesview, void*);
    void (*Request)(xhttpserver*, xhttpconn*, const xhttpserverrequest*, const xhttprouteparam*, size_t, void*);
    void (*Release)(void*);
    void* Data;
} xhttpserverrouteevents;

typedef struct xtaskpoolconfig {
    uint32_t Threads;
    size_t QueueLimit;
    size_t StackSize;
} xtaskpoolconfig;

typedef struct xhttpstaticpathconfig {
    xstrview Mount;
    uint32_t Flags;
} xhttpstaticpathconfig;

typedef struct xhttpstaticplanconfig {
    size_t MaxRanges;
    uint64_t MergeGap;
} xhttpstaticplanconfig;

typedef struct xhttpstaticreplyconfig {
    xhttpstaticplanconfig Plan;
    xstrview ContentType;
    xstrview CacheControl;
    xstrview Boundary;
} xhttpstaticreplyconfig;

typedef struct xhttpstaticserveconfig {
    xhttpstaticpathconfig Path;
    xhttpstaticreplyconfig Reply;
    const xstrview* Indexes;
    size_t IndexCount;
} xhttpstaticserveconfig;

typedef struct xtlsserverrequest {
    xbytesview ServerName;
    xbytesview Protocols;
} xtlsserverrequest;

typedef struct xtlsserverchoice {
    const xtlsidentity* Identity;
    size_t Protocol;
} xtlsserverchoice;

typedef struct xtlsserverresumerequest {
    xbytesview ServerName;
    xbytesview Protocols;
    xbytesview Ticket;
    uint32_t Age;
} xtlsserverresumerequest;

typedef bool (*xl6_tls_select_proc)(void*, const xtlsserverrequest*, xtlsserverchoice*);
typedef const xtlsresume* (*xl6_tls_resume_proc)(void*, const xtlsserverresumerequest*);

typedef struct xtlsserverconfig {
    const xtlscontext* Context;
    const xtlsidentity* Identity;
    const xstrview* Protocols;
    size_t ProtocolCount;
    xl6_tls_select_proc Select;
    void* SelectContext;
    bool RequireProtocol;
    xl6_tls_resume_proc Resume;
    void* ResumeContext;
    uint32_t ResumeAgeTolerance;
} xtlsserverconfig;

typedef struct xtlsstreamconfig {
    uint64_t HandshakeTimeout;
    uint64_t CloseTimeout;
    size_t AsyncBytesLimit;
    uint32_t AsyncCountLimit;
    uint32_t AsyncBatch;
} xtlsstreamconfig;

typedef struct xhttpservertlsconfig {
    xtlsserverconfig Handshake;
    xtlsstreamconfig Stream;
} xhttpservertlsconfig;

    )
    "\n"
    "#endif\n\n"
    XL6_HTTP_SERVER_SOURCE(

typedef struct XL6_Http_Server XL6_Http_Server;
typedef struct XL6_Http_Route XL6_Http_Route;

typedef struct XL6_Http_Router {
    size_t References;
    xhttpserverrouter* Router;
    XL6_Http_Route* Routes;
    XL6_Http_Route* ErrorRoute;
} XL6_Http_Router;

typedef struct XL6_Http_App {
    XL6_Http_Router* Shared;
} XL6_Http_App;

typedef struct XL6_Http_Host {
    char* Host;
    XL6_Http_Router* Shared;
    struct XL6_Http_Host* Next;
} XL6_Http_Host;

typedef struct XL6_Http_Tls_Host {
    char* Host;
    xtlsidentity* Identity;
    struct XL6_Http_Tls_Host* Next;
} XL6_Http_Tls_Host;

typedef struct XL6_Http_Static {
    xroot Root;
    char* Mount;
    char* IndexStorage;
    xstrview* Indexes;
    size_t IndexCount;
    char* CacheControl;
    xhttpstaticserveconfig Config;
} XL6_Http_Static;

struct XL6_Http_Server {
    xnetengine* Engine;
    bool OwnEngine;
    xhttpserver* Native;
    xhttpservermux* Mux;
    XL6_Http_Router* Default;
    XL6_Http_Host* Hosts;
    XL6_Http_Tls_Host* TlsHosts;
    xtaskpool* Pool;
    xtlsidentity* Identity;
    xhttpservertlsconfig Tls;
    xhttpserverconfig Config;
    size_t MultipartParts;
    size_t MultipartPartBytes;
    bool Secure;
    bool HasTlsConfig;
};

enum {
    XL6_HTTP_ROUTE_TEXT = 1,
    XL6_HTTP_ROUTE_OBJECT,
    XL6_HTTP_ROUTE_STREAM,
    XL6_HTTP_ROUTE_ERROR,
    XL6_HTTP_ROUTE_STATIC
};

struct XL6_Http_Route {
    int Kind;
    XL6_Function Handler;
    XL6_Function BeginHandler;
    XL6_Function ChunkHandler;
    XL6_Function EndHandler;
    XL6_Http_Server* Owner;
    XL6_Http_Static* Static;
    XL6_Http_Route* Next;
};

typedef struct XL6_Http_Request {
    const xhttpserverrequest* Request;
    xhttpconn* Connection;
    const xhttprouteparam* Params;
    size_t ParamCount;
    XL6_Http_Server* Owner;
    xhttptarget Target;
    bool TargetReady;
    bool TargetTried;
    xqueryparams* Query;
    bool QueryTried;
    xqueryparams* Form;
    bool FormTried;
    xformdata* FormData;
    bool FormDataTried;
    xcookiepair* Cookies;
    size_t CookieCount;
    bool CookiesTried;
    uint16_t ErrorStatus;
} XL6_Http_Request;

typedef struct XL6_Http_Response {
    XL6_Http_Server* Owner;
    XL6_Http_Request* Request;
    xhttpconn* Connection;
    xhttpreply* Reply;
    xbuffer* Buffer;
    bool Committed;
    bool Started;
} XL6_Http_Response;
    )
    "\n";



/* 只声明当前公开 API；具体实现由 xlang 打包的 XRT 提供。 */
static const char xl6HttpServerApiSource[] = XL6_HTTP_SERVER_SOURCE(
static inline char* xl6_xrt_string_dup(const char*, size_t);
static bool xl6_net_address_parse(xnetaddr*, const char*, uint16_t, bool);
    ) "\n";



/* 无歧义的视图、所有权和路由生命周期辅助函数。 */
