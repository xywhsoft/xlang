/* upnp_native.c - UPnP SSDP protocol utilities */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[4096];
static char err_buf[256];

const char *xupnp_error(void) { return err_buf; }

/* SSDP multicast address */
const char *xupnp_ssdp_addr(void) { return "239.255.255.250"; }

/* SSDP port */
long long xupnp_ssdp_port(void) { return 1900; }

/* Build M-SEARCH discovery packet */
const char *xupnp_build_msearch(const char *target) {
    sprintf(buf,
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: %s\r\n"
        "\r\n", target);
    return buf;
}

/* Build NOTIFY alive packet */
const char *xupnp_build_notify(const char *usn, const char *location, const char *nt) {
    sprintf(buf,
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: %s\r\n"
        "NT: %s\r\n"
        "NTS: ssdp:alive\r\n"
        "SERVER: xlang/1.0 UPnP/1.1\r\n"
        "USN: %s\r\n"
        "\r\n", location, nt, usn);
    return buf;
}

/* Parse a header value from an HTTP response */
const char *xupnp_parse_header(const char *response, const char *header) {
    static char val_buf[512];
    const char *p = strstr(response, header);
    if (!p) return "";
    p += strlen(header);
    while (*p == ' ' || *p == ':') p++;
    int i = 0;
    while (*p && *p != '\r' && *p != '\n' && i < 511)
        val_buf[i++] = *p++;
    val_buf[i] = '\0';
    return val_buf;
}

/* Common UPnP device types */
const char *xupnp_type_root(void) { return "upnp:rootdevice"; }
const char *xupnp_type_igd(void) { return "urn:schemas-upnp-org:device:InternetGatewayDevice:1"; }
const char *xupnp_type_wan(void) { return "urn:schemas-upnp-org:service:WANIPConnection:1"; }

/* Validate a UPnP response contains required headers */
long long xupnp_valid_response(const char *response) {
    if (!response || strlen(response) == 0) return 0;
    if (!strstr(response, "HTTP/1.1 200")) return 0;
    if (!strstr(response, "LOCATION:")) return 0;
    if (!strstr(response, "ST:") && !strstr(response, "NT:")) return 0;
    return 1;
}

/* Extract location from response */
const char *xupnp_get_location(const char *response) {
    return xupnp_parse_header(response, "LOCATION");
}

/* Extract USN from response */
const char *xupnp_get_usn(const char *response) {
    return xupnp_parse_header(response, "USN");
}
