xhttp xlang deferred assets

This directory preserves the advanced HTTP language layer removed from the
xlang core product: URL/query/form helpers, semantic objects, high-level
client/server generators, tests, examples, and dependent projects.

The saved stdlib source and table fragments are migration input, not active
core code. They must be adapted to the public xhttp extension API and its
package/link entry points before becoming active again. They must not be
restored by adding xhttp symbols back to the trimmed XRT core snapshot.

Preserved examples:

- examples/stdlib_http_client.xl
- examples/stdlib_http_client_c.c
- examples/stdlib_http_server.xl
- examples/stdlib_http_server_c.c
- examples/stdlib_http_server_app.xl

Preserved dependent projects:

- projects/chatroom
- projects/crawl
- projects/dbengine
- projects/fileserver
- projects/httpreq
- projects/logagg (also requires xregex)
- projects/procmgr
- projects/ratelimit
- projects/webfw
- projects/webproxy
- projects/xblog
- projects/xdistdb
- projects/xmonitor
- projects/xsitegen

Project-local commands still use their former release/projects/dev paths and
must be rewritten when the xhttp package import and link contract is finalized.
