/* gpio_native.c - GPIO control (simulated for portability, sysfs on Linux) */
#include <string.h>
#include <stdlib.h>

#define MAX_PINS 64

/* Pin modes */
#define GPIO_INPUT  0
#define GPIO_OUTPUT 1

/* Pin states */
typedef struct {
    int exported;
    int direction;  /* 0=input, 1=output */
    int value;      /* 0 or 1 */
    int pull;       /* 0=none, 1=up, 2=down */
} PinState;

typedef struct {
    PinState pins[MAX_PINS];
    int initialized;
} GpioController;

static char err_buf[256];

const char *xgpio_error(void) { return err_buf; }

void *xgpio_init(void) {
    GpioController *gc = (GpioController *)calloc(1, sizeof(GpioController));
    gc->initialized = 1;
    return gc;
}

void xgpio_cleanup(void *handle) {
    GpioController *gc = (GpioController *)handle;
    if (gc) free(gc);
}

long long xgpio_export(void *handle, long long pin) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    gc->pins[pin].exported = 1;
    gc->pins[pin].value = 0;
    gc->pins[pin].direction = GPIO_INPUT;
    return 1;
}

long long xgpio_unexport(void *handle, long long pin) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    gc->pins[pin].exported = 0;
    return 1;
}

long long xgpio_set_direction(void *handle, long long pin, long long dir) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    if (!gc->pins[pin].exported) return 0;
    gc->pins[pin].direction = (int)dir;
    return 1;
}

long long xgpio_get_direction(void *handle, long long pin) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return -1;
    if (!gc->pins[pin].exported) return -1;
    return gc->pins[pin].direction;
}

long long xgpio_write(void *handle, long long pin, long long value) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    if (!gc->pins[pin].exported) return 0;
    if (gc->pins[pin].direction != GPIO_OUTPUT) return 0;
    gc->pins[pin].value = value ? 1 : 0;
    return 1;
}

long long xgpio_read(void *handle, long long pin) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return -1;
    if (!gc->pins[pin].exported) return -1;
    /* For input pins with pull-up, default high */
    if (gc->pins[pin].direction == GPIO_INPUT && gc->pins[pin].pull == 1)
        return 1;
    return gc->pins[pin].value;
}

long long xgpio_set_pull(void *handle, long long pin, long long pull) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    if (!gc->pins[pin].exported) return 0;
    gc->pins[pin].pull = (int)pull;
    return 1;
}

long long xgpio_is_exported(void *handle, long long pin) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    return gc->pins[pin].exported;
}

/* Toggle output pin */
long long xgpio_toggle(void *handle, long long pin) {
    GpioController *gc = (GpioController *)handle;
    if (!gc || pin < 0 || pin >= MAX_PINS) return 0;
    if (!gc->pins[pin].exported) return 0;
    if (gc->pins[pin].direction != GPIO_OUTPUT) return 0;
    gc->pins[pin].value = !gc->pins[pin].value;
    return gc->pins[pin].value;
}
