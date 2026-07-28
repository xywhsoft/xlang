/*
 * kubernetes_native.c - Kubernetes API helpers
 * Tier 4: DevOps
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xk8s_resource_type(long long id) {
    switch (id) {
        case 0: return "Pod";
        case 1: return "Deployment";
        case 2: return "Service";
        case 3: return "ConfigMap";
        case 4: return "Secret";
        case 5: return "Namespace";
        case 6: return "Node";
        case 7: return "StatefulSet";
        case 8: return "DaemonSet";
        case 9: return "Job";
        case 10: return "CronJob";
        case 11: return "Ingress";
        case 12: return "PersistentVolumeClaim";
        case 13: return "ServiceAccount";
        case 14: return "Role";
        case 15: return "RoleBinding";
        default: return "Unknown";
    }
}

const char* xk8s_pod_phase(long long id) {
    switch (id) {
        case 0: return "Pending";
        case 1: return "Running";
        case 2: return "Succeeded";
        case 3: return "Failed";
        case 4: return "Unknown";
        default: return "Unknown";
    }
}

const char* xk8s_service_type(long long id) {
    switch (id) {
        case 0: return "ClusterIP";
        case 1: return "NodePort";
        case 2: return "LoadBalancer";
        case 3: return "ExternalName";
        default: return "Unknown";
    }
}

const char* xk8s_restart_policy(long long id) {
    switch (id) {
        case 0: return "Always";
        case 1: return "OnFailure";
        case 2: return "Never";
        default: return "Unknown";
    }
}

const char* xk8s_api_group(long long id) {
    switch (id) {
        case 0: return "core/v1";
        case 1: return "apps/v1";
        case 2: return "batch/v1";
        case 3: return "networking.k8s.io/v1";
        case 4: return "rbac.authorization.k8s.io/v1";
        case 5: return "storage.k8s.io/v1";
        case 6: return "autoscaling/v2";
        default: return "Unknown";
    }
}

/* Parse resource quantity like "100m" (millicores) or "2Gi" */
long long xk8s_parse_cpu_milli(const char *qty) {
    long long val = atol(qty);
    const char *p = qty;
    while (*p >= '0' && *p <= '9') p++;
    if (*p == 'm') return val;
    return val * 1000;
}

const char* xk8s_condition_type(long long id) {
    switch (id) {
        case 0: return "Ready";
        case 1: return "PodScheduled";
        case 2: return "Initialized";
        case 3: return "ContainersReady";
        case 4: return "Available";
        case 5: return "Progressing";
        case 6: return "ReplicaFailure";
        default: return "Unknown";
    }
}
