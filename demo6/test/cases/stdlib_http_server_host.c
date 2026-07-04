#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

void* xl6_http_server_create(void);
int64_t xl6_http_server_port(void* server);
void xl6_http_server_stop(void* server);

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

static int request_once(int64_t port, const char* request, const char* expected)
{
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in addr;
    char response[4096];
    int ok = 0;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( sock == INVALID_SOCKET ) {
        return 0;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ( connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
        closesocket(sock);
        return 0;
    }

    if ( send_all(sock, request, (int)strlen(request)) ) {
        ok = recv_response(sock, response, (int)sizeof(response), expected);
    }

    closesocket(sock);
    return ok;
}

int main(void)
{
    WSADATA wsa;
    char request[512];
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
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    ok = ok && request_once(port, request, "route-get-ok");

    snprintf(request, sizeof(request),
        "POST /demo?x=1 HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "ping",
        (long long)port);
    ok = ok && request_once(port, request, "xlang-http-ok");

    snprintf(request, sizeof(request),
        "PUT /any HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        (long long)port);
    ok = ok && request_once(port, request, "route-any-ok");

    snprintf(request, sizeof(request),
        "GET /public/index.txt HTTP/1.1\r\n"
        "Host: 127.0.0.1:%lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        (long long)port);
    ok = ok && request_once(port, request, "static-ok");

    xl6_http_server_stop(server);
    xlUnit();
    WSACleanup();
    return ok ? 0 : 14;
}
