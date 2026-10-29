typedef struct LeaseDropHooks {
    void (*inspect)(void*);
    void* context;
    xerror* error;
    void (*dispose)(void*, xerror*);
} LeaseDropHooks;
