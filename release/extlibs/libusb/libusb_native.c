/* libusb_native.c - Simulated USB device enumeration and transfers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[4096];

/* Simulated device table */
typedef struct { int vid; int pid; const char *name; int bus; int addr; } usb_dev;
static usb_dev sim_devices[] = {
    {0x046D, 0xC52B, "Logitech Unifying Receiver", 1, 2},
    {0x8087, 0x0024, "Intel Bluetooth Adapter", 1, 3},
    {0x0781, 0x5567, "SanDisk USB Flash Drive", 2, 1},
    {0x2109, 0x3431, "USB Hub Controller", 2, 4},
};
#define NUM_SIM_DEV 4

/* Get number of simulated devices */
long long xusb_device_count(void) { return NUM_SIM_DEV; }

/* Get device VID by index */
long long xusb_get_vid(long long index) {
    if (index < 0 || index >= NUM_SIM_DEV) return 0;
    return (long long)sim_devices[index].vid;
}

/* Get device PID by index */
long long xusb_get_pid(long long index) {
    if (index < 0 || index >= NUM_SIM_DEV) return 0;
    return (long long)sim_devices[index].pid;
}

/* Get device name by index */
const char *xusb_get_name(long long index) {
    if (index < 0 || index >= NUM_SIM_DEV) return "";
    return sim_devices[index].name;
}

/* Get device bus number */
long long xusb_get_bus(long long index) {
    if (index < 0 || index >= NUM_SIM_DEV) return 0;
    return (long long)sim_devices[index].bus;
}

/* Get device address */
long long xusb_get_addr(long long index) {
    if (index < 0 || index >= NUM_SIM_DEV) return 0;
    return (long long)sim_devices[index].addr;
}

/* Find device by VID:PID, returns index or -1 */
long long xusb_find(long long vid, long long pid) {
    int i;
    for (i = 0; i < NUM_SIM_DEV; i++)
        if (sim_devices[i].vid == vid && sim_devices[i].pid == pid) return i;
    return -1;
}

/* Format VID:PID as string */
const char *xusb_format_id(long long vid, long long pid) {
    sprintf(buf, "%04x:%04x", (int)vid, (int)pid);
    return buf;
}

/* Build USB control transfer request descriptor (hex) */
const char *xusb_control_request(long long bmRequestType, long long bRequest, long long wValue, long long wIndex, long long wLength) {
    unsigned char req[8];
    req[0] = (unsigned char)bmRequestType;
    req[1] = (unsigned char)bRequest;
    req[2] = (unsigned char)(wValue & 0xFF);
    req[3] = (unsigned char)((wValue >> 8) & 0xFF);
    req[4] = (unsigned char)(wIndex & 0xFF);
    req[5] = (unsigned char)((wIndex >> 8) & 0xFF);
    req[6] = (unsigned char)(wLength & 0xFF);
    req[7] = (unsigned char)((wLength >> 8) & 0xFF);
    int i; for (i = 0; i < 8; i++) sprintf(buf+i*2, "%02x", req[i]);
    buf[16] = '\0';
    return buf;
}

/* Get USB class name */
const char *xusb_class_name(long long class_code) {
    switch(class_code) {
        case 0x01: return "Audio";
        case 0x02: return "Communications";
        case 0x03: return "HID";
        case 0x07: return "Printer";
        case 0x08: return "Mass Storage";
        case 0x09: return "Hub";
        case 0x0A: return "CDC Data";
        case 0x0E: return "Video";
        case 0xFF: return "Vendor Specific";
        default: return "Unknown";
    }
}

/* Get descriptor type name */
const char *xusb_descriptor_name(long long desc_type) {
    switch(desc_type) {
        case 1: return "Device";
        case 2: return "Configuration";
        case 3: return "String";
        case 4: return "Interface";
        case 5: return "Endpoint";
        case 6: return "Device Qualifier";
        default: return "Unknown";
    }
}
