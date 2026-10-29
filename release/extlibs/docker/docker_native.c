/*
 * docker_native.c - Docker API helpers
 * Tier 4: DevOps
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xdocker_container_state(long long id) {
    switch (id) {
        case 0: return "created";
        case 1: return "running";
        case 2: return "paused";
        case 3: return "restarting";
        case 4: return "removing";
        case 5: return "exited";
        case 6: return "dead";
        default: return "Unknown";
    }
}

const char* xdocker_network_mode(long long id) {
    switch (id) {
        case 0: return "bridge";
        case 1: return "host";
        case 2: return "none";
        case 3: return "overlay";
        case 4: return "macvlan";
        default: return "Unknown";
    }
}

const char* xdocker_restart_policy(long long id) {
    switch (id) {
        case 0: return "no";
        case 1: return "always";
        case 2: return "on-failure";
        case 3: return "unless-stopped";
        default: return "Unknown";
    }
}

const char* xdocker_volume_driver(long long id) {
    switch (id) {
        case 0: return "local";
        case 1: return "nfs";
        case 2: return "tmpfs";
        case 3: return "azurefile";
        case 4: return "gce-pd";
        default: return "Unknown";
    }
}

const char* xdocker_api_endpoint(long long id) {
    switch (id) {
        case 0: return "/containers/json";
        case 1: return "/images/json";
        case 2: return "/networks";
        case 3: return "/volumes";
        case 4: return "/info";
        case 5: return "/version";
        case 6: return "/events";
        case 7: return "/system/df";
        default: return "Unknown";
    }
}

/* Parse port mapping "host:container" and return container port */
long long xdocker_parse_port(const char *mapping) {
    const char *p = mapping;
    while (*p && *p != ':') p++;
    if (*p == ':') return atol(p + 1);
    return atol(mapping);
}

const char* xdocker_log_driver(long long id) {
    switch (id) {
        case 0: return "json-file";
        case 1: return "syslog";
        case 2: return "journald";
        case 3: return "gelf";
        case 4: return "fluentd";
        case 5: return "awslogs";
        case 6: return "none";
        default: return "Unknown";
    }
}
