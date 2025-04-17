typedef struct _COPY_MEMORY {
    pid_t pid;
    uintptr_t addr;
    void* buffer;
    size_t size;
} COPY_MEMORY, *PCOPY_MEMORY;

typedef struct _MODULE_BASE {
    pid_t pid;
    char* name;
    uintptr_t base;
} MODULE_BASE, *PMODULE_BASE;

enum OPERATIONS {
    OP_INIT_KEY = 0x8800,
    OP_READ_MEM = 0x8801,
    OP_WRITE_MEM = 0x8802,
    OP_MODULE_BASE = 0x8803,
    OP_RW_MEM = 0x8804,
};
