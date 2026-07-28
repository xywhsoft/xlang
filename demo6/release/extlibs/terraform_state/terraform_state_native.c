/*
 * terraform_state_native.c - Terraform state management
 * Tier 4: devops
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xtf_resource_type(long long id) {
    switch (id) {
        case 0: return "aws_instance";
        case 1: return "aws_s3_bucket";
        case 2: return "aws_vpc";
        case 3: return "google_compute_instance";
        case 4: return "azurerm_virtual_machine";
        case 5: return "kubernetes_deployment";
        case 6: return "docker_container";
        case 7: return "null_resource";
        default: return "Unknown";
    }
}

const char* xtf_state_action(long long id) {
    switch (id) {
        case 0: return "create";
        case 1: return "read";
        case 2: return "update";
        case 3: return "delete";
        case 4: return "no-op";
        default: return "Unknown";
    }
}

const char* xtf_backend(long long id) {
    switch (id) {
        case 0: return "local";
        case 1: return "s3";
        case 2: return "gcs";
        case 3: return "azurerm";
        case 4: return "consul";
        case 5: return "remote";
        default: return "Unknown";
    }
}

const char* xtf_lock_info(long long id) {
    switch (id) {
        case 0: return "ID";
        case 1: return "Operation";
        case 2: return "Info";
        case 3: return "Who";
        case 4: return "Version";
        case 5: return "Created";
        case 6: return "Path";
        default: return "Unknown";
    }
}

/* Validate a terraform resource address like "aws_instance.web" */
long long xtf_validate_address(const char *addr) {
    const char *dot = strchr(addr, '.');
    if (!dot) return 0;
    if (dot == addr) return 0;
    if (*(dot + 1) == '\0') return 0;
    return 1;
}

/* Compute state version from serial number */
const char* xtf_state_version(long long serial) {
    snprintf(buf, sizeof(buf), "4.%lld", serial);
    return buf;
}

const char* xtf_provider(long long id) {
    switch (id) {
        case 0: return "hashicorp/aws";
        case 1: return "hashicorp/google";
        case 2: return "hashicorp/azurerm";
        case 3: return "hashicorp/kubernetes";
        case 4: return "hashicorp/docker";
        case 5: return "hashicorp/null";
        case 6: return "hashicorp/local";
        case 7: return "hashicorp/random";
        default: return "Unknown";
    }
}
