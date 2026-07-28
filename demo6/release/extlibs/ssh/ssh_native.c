/*
 * ssh_native.c - SSH protocol helpers
 * Tier 3: Secure Shell protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* SSH message type */
const char* xssh_msg_type(long long type) {
    switch (type) {
        case 1: return "SSH_MSG_DISCONNECT";
        case 2: return "SSH_MSG_IGNORE";
        case 3: return "SSH_MSG_UNIMPLEMENTED";
        case 4: return "SSH_MSG_DEBUG";
        case 5: return "SSH_MSG_SERVICE_REQUEST";
        case 6: return "SSH_MSG_SERVICE_ACCEPT";
        case 7: return "SSH_MSG_EXT_INFO";
        case 20: return "SSH_MSG_KEXINIT";
        case 21: return "SSH_MSG_NEWKEYS";
        case 30: return "SSH_MSG_KEXDH_INIT";
        case 31: return "SSH_MSG_KEXDH_REPLY";
        case 50: return "SSH_MSG_USERAUTH_REQUEST";
        case 51: return "SSH_MSG_USERAUTH_FAILURE";
        case 52: return "SSH_MSG_USERAUTH_SUCCESS";
        case 53: return "SSH_MSG_USERAUTH_BANNER";
        case 60: return "SSH_MSG_USERAUTH_PK_OK";
        case 80: return "SSH_MSG_GLOBAL_REQUEST";
        case 81: return "SSH_MSG_REQUEST_SUCCESS";
        case 82: return "SSH_MSG_REQUEST_FAILURE";
        case 90: return "SSH_MSG_CHANNEL_OPEN";
        case 91: return "SSH_MSG_CHANNEL_OPEN_CONFIRMATION";
        case 92: return "SSH_MSG_CHANNEL_OPEN_FAILURE";
        case 93: return "SSH_MSG_CHANNEL_WINDOW_ADJUST";
        case 94: return "SSH_MSG_CHANNEL_DATA";
        case 95: return "SSH_MSG_CHANNEL_EXTENDED_DATA";
        case 96: return "SSH_MSG_CHANNEL_EOF";
        case 97: return "SSH_MSG_CHANNEL_CLOSE";
        case 98: return "SSH_MSG_CHANNEL_REQUEST";
        case 99: return "SSH_MSG_CHANNEL_SUCCESS";
        case 100: return "SSH_MSG_CHANNEL_FAILURE";
        default: return "UNKNOWN";
    }
}

/* Disconnect reason */
const char* xssh_disconnect_reason(long long code) {
    switch (code) {
        case 1: return "HOST_NOT_ALLOWED_TO_CONNECT";
        case 2: return "PROTOCOL_ERROR";
        case 3: return "KEY_EXCHANGE_FAILED";
        case 4: return "RESERVED";
        case 5: return "MAC_ERROR";
        case 6: return "COMPRESSION_ERROR";
        case 7: return "SERVICE_NOT_AVAILABLE";
        case 8: return "PROTOCOL_VERSION_NOT_SUPPORTED";
        case 9: return "HOST_KEY_NOT_VERIFIABLE";
        case 10: return "CONNECTION_LOST";
        case 11: return "BY_APPLICATION";
        case 12: return "TOO_MANY_CONNECTIONS";
        case 13: return "AUTH_CANCELLED_BY_USER";
        case 14: return "NO_MORE_AUTH_METHODS_AVAILABLE";
        case 15: return "ILLEGAL_USER_NAME";
        default: return "UNKNOWN";
    }
}

/* Channel open failure reason */
const char* xssh_channel_failure(long long code) {
    switch (code) {
        case 1: return "ADMINISTRATIVELY_PROHIBITED";
        case 2: return "CONNECT_FAILED";
        case 3: return "UNKNOWN_CHANNEL_TYPE";
        case 4: return "RESOURCE_SHORTAGE";
        default: return "UNKNOWN";
    }
}

/* Key exchange algorithm name */
const char* xssh_kex_name(long long id) {
    switch (id) {
        case 0: return "diffie-hellman-group1-sha1";
        case 1: return "diffie-hellman-group14-sha1";
        case 2: return "diffie-hellman-group14-sha256";
        case 3: return "diffie-hellman-group16-sha512";
        case 4: return "diffie-hellman-group18-sha512";
        case 5: return "curve25519-sha256";
        case 6: return "ecdh-sha2-nistp256";
        case 7: return "ecdh-sha2-nistp384";
        case 8: return "ecdh-sha2-nistp521";
        default: return "unknown";
    }
}

/* Cipher algorithm name */
const char* xssh_cipher_name(long long id) {
    switch (id) {
        case 0: return "aes128-ctr";
        case 1: return "aes192-ctr";
        case 2: return "aes256-ctr";
        case 3: return "aes128-gcm@openssh.com";
        case 4: return "aes256-gcm@openssh.com";
        case 5: return "chacha20-poly1305@openssh.com";
        case 6: return "3des-cbc";
        case 7: return "blowfish-cbc";
        case 8: return "aes128-cbc";
        case 9: return "aes256-cbc";
        default: return "unknown";
    }
}

/* MAC algorithm name */
const char* xssh_mac_name(long long id) {
    switch (id) {
        case 0: return "hmac-sha2-256";
        case 1: return "hmac-sha2-512";
        case 2: return "hmac-sha1";
        case 3: return "hmac-sha2-256-etm@openssh.com";
        case 4: return "hmac-sha2-512-etm@openssh.com";
        case 5: return "hmac-sha1-etm@openssh.com";
        default: return "unknown";
    }
}

/* Host key algorithm name */
const char* xssh_hostkey_name(long long id) {
    switch (id) {
        case 0: return "ssh-rsa";
        case 1: return "rsa-sha2-256";
        case 2: return "rsa-sha2-512";
        case 3: return "ssh-ed25519";
        case 4: return "ecdsa-sha2-nistp256";
        case 5: return "ecdsa-sha2-nistp384";
        case 6: return "ecdsa-sha2-nistp521";
        case 7: return "ssh-dss";
        default: return "unknown";
    }
}

/* Auth method name */
const char* xssh_auth_method(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "password";
        case 2: return "publickey";
        case 3: return "keyboard-interactive";
        case 4: return "hostbased";
        default: return "unknown";
    }
}
