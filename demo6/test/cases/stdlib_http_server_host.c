#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

void* xl6_http_server_create(void);
void* xl6_http_server_create_limited(void);
void* xl6_http_server_create_header_limited(void);
void* xl6_http_server_create_multipart_limited(void);
void* xl6_http_server_create_timeout(void);
int64_t xl6_http_server_port(void* server);
void xl6_http_server_stop(void* server);
int xl6_http_server_drain(void* server, int64_t timeoutMs);
int xl6_http_server_remove_host(void* server);
int64_t xl6_http_server_request_count(void* server);
int64_t xl6_http_server_connection_count(void* server);

typedef struct XL_InitOptions {
    unsigned int size;
    unsigned int version;
    int argc;
    char** argv;
    void* reserved;
} XL_InitOptions;

XL_InitOptions xlInitOptions(int argc, char** argv);
int xlInit(const XL_InitOptions* options);
void xlUnit(void);

static int send_all(SOCKET sock, const char* data, int size)
{
    int sent = 0;

    while ( sent < size ) {
        int n = send(sock, data + sent, size - sent, 0);
        if ( n <= 0 ) {
            return 0;
        }
        sent += n;
    }
    return 1;
}

static int recv_response(SOCKET sock, char* out, int cap, const char* expected)
{
    int total = 0;

    while ( total + 1 < cap ) {
        int n = recv(sock, out + total, cap - total - 1, 0);
        if ( n <= 0 ) {
            break;
        }
        total += n;
        out[total] = '\0';
        if ( strstr(out, expected) != NULL ) {
            return 1;
        }
    }
    out[total] = '\0';
    return strstr(out, expected) != NULL;
}

static SOCKET connect_local_http(int64_t port, int timeoutMs)
{
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( sock == INVALID_SOCKET ) {
        return INVALID_SOCKET;
    }
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, (int)sizeof(timeoutMs));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, (int)sizeof(timeoutMs));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ( connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

static int request_once(int64_t port, const char* request, const char* expected)
{
    SOCKET sock = INVALID_SOCKET;
    char response[4096];
    int ok = 0;

    sock = connect_local_http(port, 5000);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }

    if ( send_all(sock, request, (int)strlen(request)) ) {
        ok = recv_response(sock, response, (int)sizeof(response), expected);
    }
    if ( !ok ) {
        fprintf(stderr, "HTTP server unexpected response for expected [%s]:\n%s\n", expected, response);
    }

    closesocket(sock);
    return ok;
}

static int request_parts_once(int64_t port, const char* part1, const char* part2, const char* part3, const char* expected)
{
    SOCKET sock = INVALID_SOCKET;
    char response[4096];
    int ok = 0;

    sock = connect_local_http(port, 5000);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }

    if ( send_all(sock, part1, (int)strlen(part1)) &&
         send_all(sock, part2, (int)strlen(part2)) &&
         send_all(sock, part3, (int)strlen(part3)) ) {
        ok = recv_response(sock, response, (int)sizeof(response), expected);
    }
    if ( !ok ) {
        fprintf(stderr, "HTTP server unexpected split response for expected [%s]:\n%s\n", expected, response);
    }

    closesocket(sock);
    return ok;
}

static int request_no_send_once(int64_t port, const char* expected)
{
    SOCKET sock;
    char response[4096];
    int ok;

    sock = connect_local_http(port, 5000);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }
    ok = recv_response(sock, response, (int)sizeof(response), expected);
    if ( !ok ) {
        fprintf(stderr, "HTTP timeout test unexpected response for expected [%s]:\n%s\n", expected, response);
    }
    closesocket(sock);
    return ok;
}

static int request_partial_body_timeout(int64_t port)
{
    SOCKET sock;
    char request[512];
    char response[4096];
    int ok = 0;

    sock = connect_local_http(port, 5000);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }
    snprintf(request, sizeof(request),
        "POST /slow-body HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 8\r\n"
        "\r\n"
        "1234",
        (long long)port);
    if ( send_all(sock, request, (int)strlen(request)) ) {
        ok = recv_response(sock, response, (int)sizeof(response), "Request Timeout");
    }
    if ( !ok ) {
        fprintf(stderr, "HTTP body timeout unexpected response:\n%s\n", response);
    }
    closesocket(sock);
    return ok;
}

static int request_idle_timeout_close(int64_t port)
{
    SOCKET sock;
    char request[512];
    char response[4096];
    char one;
    int ok = 0;
    int n;

    sock = connect_local_http(port, 5000);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }
    snprintf(request, sizeof(request),
        "GET /idle HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        (long long)port);
    if ( send_all(sock, request, (int)strlen(request)) ) {
        ok = recv_response(sock, response, (int)sizeof(response), "idle-ok");
    }
    if ( ok ) {
        n = recv(sock, &one, 1, 0);
        ok = (n == 0);
    }
    if ( !ok ) {
        fprintf(stderr, "HTTP idle timeout did not close after response:\n%s\n", response);
    }
    closesocket(sock);
    return ok;
}

static void check_step(const char* label, int condition, int* ok)
{
    if ( !condition ) {
        fprintf(stderr, "HTTP server test failed: %s\n", label);
        *ok = 0;
    }
}

int main(void)
{
    WSADATA wsa;
    char request[1024];
    void* server;
    int64_t port;
    int ok = 1;
    XL_InitOptions options;

    if ( WSAStartup(MAKEWORD(2, 2), &wsa) != 0 ) {
        return 10;
    }
    options = xlInitOptions(0, NULL);
    if ( !xlInit(&options) ) {
        WSACleanup();
        return 9;
    }

    server = xl6_http_server_create();
    port = xl6_http_server_port(server);
    if ( port <= 0 || port > 65535 ) {
        xl6_http_server_stop(server);
        xlUnit();
        WSACleanup();
        return 11;
    }

    snprintf(request, sizeof(request),
        "GET /users/42?name=xlang HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Cookie: sid=abc\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("route get", request_once(port, request, "route-get-ok"), &ok);

    snprintf(request, sizeof(request),
        "POST /demo?x=1 HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "ping",
        (long long)port);
    check_step("post body", request_once(port, request, "xlang-http-ok"), &ok);

    snprintf(request, sizeof(request),
        "POST /form HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n"
        "Content-Length: 28\r\n"
        "\r\n"
        "name=alice+bob&city=shanghai",
        (long long)port);
    check_step("form body", request_once(port, request, "form-ok"), &ok);

    {
        const char* multipartBody =
            "--xlang\r\n"
            "Content-Disposition: form-data; name=\"title\"\r\n"
            "\r\n"
            "hello multipart\r\n"
            "--xlang\r\n"
            "Content-Disposition: form-data; name=\"upload\"; filename=\"note.txt\"\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "file-data\r\n"
            "--xlang--\r\n";
        snprintf(request, sizeof(request),
            "POST /multipart HTTP/1.1\r\n"
            "Host: 127.0.0.1:%lld\r\n"
            "Connection: close\r\n"
            "Content-Type: multipart/form-data; boundary=xlang\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            (long long)port,
            (int)strlen(multipartBody),
            multipartBody);
        check_step("multipart body", request_once(port, request, "multipart-ok"), &ok);
    }

    snprintf(request, sizeof(request),
        "GET /json HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("json", request_once(port, request, "{\"ok\":true}"), &ok);

    snprintf(request, sizeof(request),
        "GET /append-body HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("append body", request_once(port, request, "append-body"), &ok);

    snprintf(request, sizeof(request),
        "POST /bytes HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "pong",
        (long long)port);
    check_step("bytes", request_once(port, request, "pong"), &ok);

    snprintf(request, sizeof(request),
        "POST /stream-body/upload HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "hello",
        (long long)port);
    check_step("stream body fixed", request_parts_once(port, request, "-world", "", "stream:upload:hello-world"), &ok);

    snprintf(request, sizeof(request),
        "POST /app-stream-body/app HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n",
        (long long)port);
    check_step("stream body chunked app", request_parts_once(port, request, "6\r\n-world\r\n", "0\r\n\r\n", "stream:app:hello-world"), &ok);

    snprintf(request, sizeof(request),
        "PUT /app-any-stream-body/app-any HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "any",
        (long long)port);
    check_step("stream body app any", request_parts_once(port, request, "-stream", "", "stream:app-any:any-stream"), &ok);

    snprintf(request, sizeof(request),
        "PATCH /app-route-stream-body/app-route HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nroute\r\n",
        (long long)port);
    check_step("stream body app route", request_parts_once(port, request, "5\r\n-body\r\n", "0\r\n\r\n", "stream:app-route:route-body"), &ok);

    snprintf(request, sizeof(request),
        "PUT /stream-put/route HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "4\r\nping\r\n",
        (long long)port);
    check_step("stream body generic", request_parts_once(port, request, "4\r\npong\r\n", "0\r\n\r\n", "stream:route:pingpong"), &ok);

    snprintf(request, sizeof(request),
        "PATCH /stream-any/any HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "stream",
        (long long)port);
    check_step("stream body any", request_parts_once(port, request, "-body", "", "stream:any:stream-body"), &ok);

    snprintf(request, sizeof(request),
        "GET /file HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("file", request_once(port, request, "file-ok"), &ok);

    snprintf(request, sizeof(request),
        "GET /redirect HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("redirect", request_once(port, request, "Location: /target"), &ok);

    snprintf(request, sizeof(request),
        "GET /cookie HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("cookie body", request_once(port, request, "cookie-ok"), &ok);
    check_step("cookie header", request_once(port, request, "Set-Cookie: sid=xyz; Path=/; Max-Age=3600; SameSite=Lax; Secure; HttpOnly"), &ok);

    snprintf(request, sizeof(request),
        "GET /delete-cookie HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("delete cookie body", request_once(port, request, "delete-cookie-ok"), &ok);
    check_step("delete cookie header", request_once(port, request, "Set-Cookie: sid=; Path=/; Max-Age=0"), &ok);

    snprintf(request, sizeof(request),
        "GET /stream HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("stream", request_once(port, request, "stream-"), &ok);

    snprintf(request, sizeof(request),
        "PUT /any HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        (long long)port);
    check_step("any", request_once(port, request, "route-any-ok"), &ok);

    snprintf(request, sizeof(request),
        "OPTIONS /generic-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        (long long)port);
    check_step("generic text route", request_once(port, request, "xlang-http-bad"), &ok);

    snprintf(request, sizeof(request),
        "DELETE /generic HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        (long long)port);
    check_step("generic route", request_once(port, request, "route-generic-ok"), &ok);

    snprintf(request, sizeof(request),
        "PATCH /app-generic HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        (long long)port);
    check_step("generic app route", request_once(port, request, "app-generic-ok"), &ok);

    snprintf(request, sizeof(request),
        "OPTIONS /app-generic-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        (long long)port);
    check_step("generic app text route", request_once(port, request, "xlang-http-bad"), &ok);

    snprintf(request, sizeof(request),
        "GET /public/index.txt HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("static", request_once(port, request, "static-ok"), &ok);

    snprintf(request, sizeof(request),
        "GET /assets/ HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("app static ex index", request_once(port, request, "home-ok"), &ok);
    check_step("app static ex cache", request_once(port, request, "Cache-Control: max-age=30"), &ok);

    snprintf(request, sizeof(request),
        "GET /server-assets/ HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("server static ex index", request_once(port, request, "server-static-ok"), &ok);
    check_step("server static ex cache", request_once(port, request, "Cache-Control: max-age=45"), &ok);

    snprintf(request, sizeof(request),
        "GET /missing HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("custom error", request_once(port, request, "error-404-Not Found"), &ok);

    snprintf(request, sizeof(request),
        "GET /app HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("reload app", request_once(port, request, "app-two"), &ok);

    snprintf(request, sizeof(request),
        "GET /host HTTP/1.1\r\n"
        "Host: x.local\r\n"
        "Connection: close\r\n"
        "\r\n");
    check_step("vhost", request_once(port, request, "host-ok"), &ok);

    check_step("remove host", xl6_http_server_remove_host(server), &ok);

    snprintf(request, sizeof(request),
        "GET /app HTTP/1.1\r\n"
        "Host: x.local\r\n"
        "Connection: close\r\n"
        "\r\n");
    check_step("remove host fallback", request_once(port, request, "app-two"), &ok);

    check_step("request count", xl6_http_server_request_count(server) >= 1, &ok);
    check_step("connection count", xl6_http_server_connection_count(server) >= 0, &ok);
    check_step("drain", xl6_http_server_drain(server, 1000), &ok);

    xl6_http_server_stop(server);

    server = xl6_http_server_create_limited();
    port = xl6_http_server_port(server);
    if ( port <= 0 || port > 65535 ) {
        xl6_http_server_stop(server);
        xlUnit();
        WSACleanup();
        return 12;
    }
    snprintf(request, sizeof(request),
        "POST /limited HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 16\r\n"
        "\r\n"
        "0123456789abcdef",
        (long long)port);
    check_step("body limit 413", request_once(port, request, "Payload Too Large"), &ok);
    xl6_http_server_stop(server);

    server = xl6_http_server_create_header_limited();
    port = xl6_http_server_port(server);
    if ( port <= 0 || port > 65535 ) {
        xl6_http_server_stop(server);
        xlUnit();
        WSACleanup();
        return 13;
    }
    snprintf(request, sizeof(request),
        "GET /headers HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "X-A: 1\r\n"
        "X-B: 2\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    check_step("header limit 431", request_once(port, request, "Request Header Fields Too Large"), &ok);
    xl6_http_server_stop(server);

    server = xl6_http_server_create_multipart_limited();
    port = xl6_http_server_port(server);
    if ( port <= 0 || port > 65535 ) {
        xl6_http_server_stop(server);
        xlUnit();
        WSACleanup();
        return 16;
    }
    {
        const char* multipartBody =
            "--xlimit\r\n"
            "Content-Disposition: form-data; name=\"data\"\r\n"
            "\r\n"
            "abcde\r\n"
            "--xlimit--\r\n";
        snprintf(request, sizeof(request),
            "POST /multipart-limit HTTP/1.1\r\n"
            "Host: 127.0.0.1:%lld\r\n"
            "Connection: close\r\n"
            "Content-Type: multipart/form-data; boundary=xlimit\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            (long long)port,
            (int)strlen(multipartBody),
            multipartBody);
        check_step("multipart part size limit 413", request_once(port, request, "Payload Too Large"), &ok);
    }
    {
        const char* multipartBody =
            "--xlimit\r\n"
            "Content-Disposition: form-data; name=\"a\"\r\n"
            "\r\n"
            "1\r\n"
            "--xlimit\r\n"
            "Content-Disposition: form-data; name=\"b\"\r\n"
            "\r\n"
            "2\r\n"
            "--xlimit--\r\n";
        snprintf(request, sizeof(request),
            "POST /multipart-limit HTTP/1.1\r\n"
            "Host: 127.0.0.1:%lld\r\n"
            "Connection: close\r\n"
            "Content-Type: multipart/form-data; boundary=xlimit\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            (long long)port,
            (int)strlen(multipartBody),
            multipartBody);
        check_step("multipart part count limit 413", request_once(port, request, "Payload Too Large"), &ok);
    }
    xl6_http_server_stop(server);

    server = xl6_http_server_create_timeout();
    port = xl6_http_server_port(server);
    if ( port <= 0 || port > 65535 ) {
        xl6_http_server_stop(server);
        xlUnit();
        WSACleanup();
        return 15;
    }
    check_step("header timeout 408", request_no_send_once(port, "Request Timeout"), &ok);
    check_step("body timeout 408", request_partial_body_timeout(port), &ok);
    check_step("idle timeout close", request_idle_timeout_close(port), &ok);
    xl6_http_server_stop(server);

    xlUnit();
    WSACleanup();
    return ok ? 0 : 14;
}
