/*
 * ldap_native.c - LDAP protocol helpers
 * Tier 3: Lightweight Directory Access Protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* LDAP result code */
const char* xldap_result_name(long long code) {
    switch (code) {
        case 0: return "success";
        case 1: return "operationsError";
        case 2: return "protocolError";
        case 3: return "timeLimitExceeded";
        case 4: return "sizeLimitExceeded";
        case 5: return "compareFalse";
        case 6: return "compareTrue";
        case 7: return "authMethodNotSupported";
        case 8: return "strongerAuthRequired";
        case 10: return "referral";
        case 11: return "adminLimitExceeded";
        case 12: return "unavailableCriticalExtension";
        case 13: return "confidentialityRequired";
        case 14: return "saslBindInProgress";
        case 16: return "noSuchAttribute";
        case 17: return "undefinedAttributeType";
        case 18: return "inappropriateMatching";
        case 19: return "constraintViolation";
        case 20: return "attributeOrValueExists";
        case 21: return "invalidAttributeSyntax";
        case 32: return "noSuchObject";
        case 33: return "aliasProblem";
        case 34: return "invalidDNSyntax";
        case 36: return "aliasDereferencingProblem";
        case 48: return "inappropriateAuthentication";
        case 49: return "invalidCredentials";
        case 50: return "insufficientAccessRights";
        case 51: return "busy";
        case 52: return "unavailable";
        case 53: return "unwillingToPerform";
        case 54: return "loopDetect";
        case 64: return "namingViolation";
        case 65: return "objectClassViolation";
        case 66: return "notAllowedOnNonLeaf";
        case 67: return "notAllowedOnRDN";
        case 68: return "entryAlreadyExists";
        case 69: return "objectClassModsProhibited";
        case 80: return "other";
        default: return "unknown";
    }
}

/* LDAP operation type */
const char* xldap_operation_name(long long op) {
    switch (op) {
        case 0: return "bindRequest";
        case 1: return "bindResponse";
        case 2: return "unbindRequest";
        case 3: return "searchRequest";
        case 4: return "searchResEntry";
        case 5: return "searchResDone";
        case 6: return "modifyRequest";
        case 7: return "modifyResponse";
        case 8: return "addRequest";
        case 9: return "addResponse";
        case 10: return "delRequest";
        case 11: return "delResponse";
        case 12: return "modDNRequest";
        case 13: return "modDNResponse";
        case 14: return "compareRequest";
        case 15: return "compareResponse";
        case 16: return "abandonRequest";
        case 19: return "searchResRef";
        case 23: return "extendedReq";
        case 24: return "extendedResp";
        case 25: return "intermediateResponse";
        default: return "unknown";
    }
}

/* LDAP search scope */
const char* xldap_scope_name(long long scope) {
    switch (scope) {
        case 0: return "baseObject";
        case 1: return "singleLevel";
        case 2: return "wholeSubtree";
        default: return "unknown";
    }
}

/* LDAP deref aliases */
const char* xldap_deref_name(long long deref) {
    switch (deref) {
        case 0: return "neverDerefAliases";
        case 1: return "derefInSearching";
        case 2: return "derefFindingBaseObj";
        case 3: return "derefAlways";
        default: return "unknown";
    }
}

/* LDAP modify operation */
const char* xldap_modify_op(long long op) {
    switch (op) {
        case 0: return "add";
        case 1: return "delete";
        case 2: return "replace";
        case 3: return "increment";
        default: return "unknown";
    }
}

/* Common LDAP attribute names */
const char* xldap_attr_desc(const char *attr) {
    if (strcmp(attr, "cn") == 0) return "Common Name";
    if (strcmp(attr, "sn") == 0) return "Surname";
    if (strcmp(attr, "givenName") == 0) return "Given Name";
    if (strcmp(attr, "mail") == 0) return "Email Address";
    if (strcmp(attr, "uid") == 0) return "User ID";
    if (strcmp(attr, "objectClass") == 0) return "Object Class";
    if (strcmp(attr, "dn") == 0) return "Distinguished Name";
    if (strcmp(attr, "dc") == 0) return "Domain Component";
    if (strcmp(attr, "ou") == 0) return "Organizational Unit";
    if (strcmp(attr, "o") == 0) return "Organization";
    if (strcmp(attr, "telephoneNumber") == 0) return "Telephone Number";
    if (strcmp(attr, "userPassword") == 0) return "User Password";
    return "Unknown Attribute";
}

/* Validate DN format (simplified) */
long long xldap_valid_dn(const char *dn) {
    int len = (int)strlen(dn);
    if (len < 3) return 0;
    /* Must contain at least one '=' */
    if (strchr(dn, '=') == NULL) return 0;
    return 1;
}
