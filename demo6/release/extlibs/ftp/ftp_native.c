/*
 * ftp_native.c - FTP protocol helpers
 * Tier 3: File Transfer Protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* FTP reply code category */
const char* xftp_reply_category(long long code) {
    int first = (int)(code / 100);
    switch (first) {
        case 1: return "Positive Preliminary";
        case 2: return "Positive Completion";
        case 3: return "Positive Intermediate";
        case 4: return "Transient Negative";
        case 5: return "Permanent Negative";
        default: return "Unknown";
    }
}

/* FTP reply code text */
const char* xftp_reply_text(long long code) {
    switch (code) {
        case 120: return "Service ready in nnn minutes";
        case 125: return "Data connection already open";
        case 150: return "File status okay; about to open data connection";
        case 200: return "Command okay";
        case 202: return "Command not implemented";
        case 211: return "System status";
        case 212: return "Directory status";
        case 213: return "File status";
        case 214: return "Help message";
        case 215: return "System type";
        case 220: return "Service ready for new user";
        case 221: return "Service closing control connection";
        case 225: return "Data connection open; no transfer in progress";
        case 226: return "Closing data connection";
        case 227: return "Entering Passive Mode";
        case 229: return "Entering Extended Passive Mode";
        case 230: return "User logged in";
        case 234: return "Security data exchange complete";
        case 250: return "Requested file action okay";
        case 257: return "PATHNAME created";
        case 331: return "User name okay, need password";
        case 332: return "Need account for login";
        case 350: return "Requested file action pending further information";
        case 421: return "Service not available";
        case 425: return "Can't open data connection";
        case 426: return "Connection closed; transfer aborted";
        case 450: return "Requested file action not taken";
        case 451: return "Requested action aborted";
        case 452: return "Requested action not taken (insufficient storage)";
        case 500: return "Syntax error, command unrecognized";
        case 501: return "Syntax error in parameters";
        case 502: return "Command not implemented";
        case 503: return "Bad sequence of commands";
        case 504: return "Command not implemented for that parameter";
        case 530: return "Not logged in";
        case 532: return "Need account for storing files";
        case 550: return "Requested action not taken (file unavailable)";
        case 551: return "Requested action aborted (page type unknown)";
        case 552: return "Requested file action aborted (exceeded storage)";
        case 553: return "Requested action not taken (file name not allowed)";
        default: return "Unknown";
    }
}

/* FTP command name */
const char* xftp_cmd_name(long long cmd) {
    switch (cmd) {
        case 0: return "USER";
        case 1: return "PASS";
        case 2: return "CWD";
        case 3: return "CDUP";
        case 4: return "PWD";
        case 5: return "LIST";
        case 6: return "NLST";
        case 7: return "RETR";
        case 8: return "STOR";
        case 9: return "APPE";
        case 10: return "DELE";
        case 11: return "MKD";
        case 12: return "RMD";
        case 13: return "RNFR";
        case 14: return "RNTO";
        case 15: return "TYPE";
        case 16: return "PASV";
        case 17: return "PORT";
        case 18: return "EPSV";
        case 19: return "EPRT";
        case 20: return "QUIT";
        case 21: return "NOOP";
        case 22: return "SYST";
        case 23: return "FEAT";
        case 24: return "OPTS";
        case 25: return "SIZE";
        case 26: return "MDTM";
        case 27: return "REST";
        case 28: return "ABOR";
        case 29: return "STAT";
        case 30: return "AUTH";
        default: return "UNKNOWN";
    }
}

/* Transfer mode */
const char* xftp_transfer_type(long long type) {
    switch (type) {
        case 0: return "ASCII";
        case 1: return "Binary (Image)";
        case 2: return "EBCDIC";
        default: return "Unknown";
    }
}

/* Parse PASV response: extract IP and port from "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)" */
const char* xftp_parse_pasv(const char *response) {
    const char *p = strchr(response, '(');
    if (!p) return "";
    p++;
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(p, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) return "";
    int port = p1 * 256 + p2;
    sprintf(buf, "%d.%d.%d.%d:%d", h1, h2, h3, h4, port);
    return buf;
}

/* Is reply positive (1xx, 2xx, 3xx)? */
long long xftp_is_positive(long long code) {
    int first = (int)(code / 100);
    return (first >= 1 && first <= 3) ? 1 : 0;
}
