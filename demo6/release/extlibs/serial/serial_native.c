/* serial_native.c - Serial port communication (Windows API + loopback simulation) */
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef struct {
    int is_open;
    int simulated;      /* loopback mode for testing */
    int baudrate;
    int databits;
    int stopbits;
    int parity;         /* 0=none, 1=odd, 2=even */
    char port_name[128];
    /* Simulation buffer */
    char sim_buf[4096];
    int sim_len;
#ifdef _WIN32
    HANDLE hPort;
#endif
} SerialPort;

static char err_buf[256];
static char read_buf[4096];

const char *xserial_error(void) { return err_buf; }

void *xserial_open(const char *port, long long baudrate, long long databits, long long stopbits, long long parity) {
    SerialPort *sp = (SerialPort *)calloc(1, sizeof(SerialPort));
    strncpy(sp->port_name, port, 127);
    sp->baudrate = (int)baudrate;
    sp->databits = (int)databits;
    sp->stopbits = (int)stopbits;
    sp->parity = (int)parity;
    sp->sim_len = 0;

    /* If port is "LOOPBACK" or "SIM", use simulation mode */
    if (strcmp(port, "LOOPBACK") == 0 || strcmp(port, "SIM") == 0) {
        sp->simulated = 1;
        sp->is_open = 1;
        return sp;
    }

#ifdef _WIN32
    {
        char full_port[160];
        DCB dcb;
        COMMTIMEOUTS timeouts;
        sprintf(full_port, "\\\\.\\%s", port);
        sp->hPort = CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (sp->hPort == INVALID_HANDLE_VALUE) {
            sprintf(err_buf, "serial: cannot open port %s", port);
            free(sp);
            return NULL;
        }
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        GetCommState(sp->hPort, &dcb);
        dcb.BaudRate = (int)baudrate;
        dcb.ByteSize = (BYTE)databits;
        dcb.StopBits = stopbits == 2 ? TWOSTOPBITS : ONESTOPBIT;
        dcb.Parity = parity == 1 ? ODDPARITY : (parity == 2 ? EVENPARITY : NOPARITY);
        dcb.fParity = parity != 0;
        SetCommState(sp->hPort, &dcb);
        memset(&timeouts, 0, sizeof(timeouts));
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.ReadTotalTimeoutMultiplier = 10;
        timeouts.WriteTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutMultiplier = 10;
        SetCommTimeouts(sp->hPort, &timeouts);
        sp->is_open = 1;
        sp->simulated = 0;
    }
#else
    sprintf(err_buf, "serial: platform not supported");
    free(sp);
    return NULL;
#endif
    return sp;
}

void xserial_close(void *handle) {
    SerialPort *sp = (SerialPort *)handle;
    if (!sp) return;
#ifdef _WIN32
    if (!sp->simulated && sp->hPort != INVALID_HANDLE_VALUE)
        CloseHandle(sp->hPort);
#endif
    sp->is_open = 0;
    free(sp);
}

long long xserial_is_open(void *handle) {
    SerialPort *sp = (SerialPort *)handle;
    return sp ? sp->is_open : 0;
}

long long xserial_write(void *handle, const char *data, long long len) {
    SerialPort *sp = (SerialPort *)handle;
    if (!sp || !sp->is_open) return -1;
    if (sp->simulated) {
        /* Loopback: append to sim buffer */
        int i;
        for (i = 0; i < (int)len && sp->sim_len < 4096; i++)
            sp->sim_buf[sp->sim_len++] = data[i];
        return len;
    }
#ifdef _WIN32
    {
        DWORD written;
        if (WriteFile(sp->hPort, data, (DWORD)len, &written, NULL))
            return (long long)written;
    }
#endif
    return -1;
}

/* Read returns data via static buffer, returns bytes read */
const char *xserial_read(void *handle, long long max_len) {
    SerialPort *sp = (SerialPort *)handle;
    if (!sp || !sp->is_open) { read_buf[0] = '\0'; return read_buf; }
    if (sp->simulated) {
        int n = sp->sim_len < (int)max_len ? sp->sim_len : (int)max_len;
        memcpy(read_buf, sp->sim_buf, n);
        read_buf[n] = '\0';
        /* Remove read bytes from sim buffer */
        memmove(sp->sim_buf, sp->sim_buf + n, sp->sim_len - n);
        sp->sim_len -= n;
        return read_buf;
    }
#ifdef _WIN32
    {
        DWORD nread;
        if (ReadFile(sp->hPort, read_buf, (DWORD)max_len - 1, &nread, NULL)) {
            read_buf[nread] = '\0';
            return read_buf;
        }
    }
#endif
    read_buf[0] = '\0';
    return read_buf;
}

long long xserial_read_len(void *handle, long long max_len) {
    SerialPort *sp = (SerialPort *)handle;
    if (!sp || !sp->is_open) return 0;
    if (sp->simulated) {
        return sp->sim_len < (int)max_len ? sp->sim_len : (int)max_len;
    }
#ifdef _WIN32
    {
        COMSTAT stat;
        DWORD errors;
        ClearCommError(sp->hPort, &errors, &stat);
        return (long long)stat.cbInQue;
    }
#endif
    return 0;
}

long long xserial_available(void *handle) {
    SerialPort *sp = (SerialPort *)handle;
    if (!sp || !sp->is_open) return 0;
    if (sp->simulated) return sp->sim_len;
#ifdef _WIN32
    {
        COMSTAT stat;
        DWORD errors;
        ClearCommError(sp->hPort, &errors, &stat);
        return (long long)stat.cbInQue;
    }
#endif
    return 0;
}

void xserial_flush(void *handle) {
    SerialPort *sp = (SerialPort *)handle;
    if (!sp || !sp->is_open) return;
    if (sp->simulated) { sp->sim_len = 0; return; }
#ifdef _WIN32
    FlushFileBuffers(sp->hPort);
#endif
}

long long xserial_get_baudrate(void *handle) {
    SerialPort *sp = (SerialPort *)handle;
    return sp ? sp->baudrate : 0;
}

const char *xserial_get_port(void *handle) {
    SerialPort *sp = (SerialPort *)handle;
    return sp ? sp->port_name : "";
}
