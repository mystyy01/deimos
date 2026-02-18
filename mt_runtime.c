// Minimal runtime symbols required by mt-lang generated objects in Deimos.
// Freestanding, no-libc: provides malloc/realloc/printf/exit.

#define SYS_EXIT   0
#define SYS_WRITE  2
#define STDOUT     1

static long mt_syscall(long num, long arg1, long arg2, long arg3, long arg4, long arg5) {
    long result;
    __asm__ volatile(
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "movq %3, %%rsi\n"
        "movq %4, %%rdx\n"
        "movq %5, %%r10\n"
        "movq %6, %%r8\n"
        "syscall\n"
        "movq %%rax, %0\n"
        : "=r"(result)
        : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r11", "rcx", "memory");
    return result;
}

static int cstr_len(const char *s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

int printf(const char *fmt, ...) {
    int len = cstr_len(fmt);
    mt_syscall(SYS_WRITE, STDOUT, (long)fmt, len, 0, 0);
    return len;
}

void exit(int status) {
    mt_syscall(SYS_EXIT, status, 0, 0, 0, 0);
    while (1) {
        __asm__ volatile("hlt");
    }
}

struct block_header {
    int size;
    int _pad0;
    int _pad1;
    int _pad2;
};

#define MT_HEAP_SIZE (256 * 1024)
static unsigned char mt_heap[MT_HEAP_SIZE] __attribute__((aligned(16)));
static int mt_heap_offset = 0;

static int align16(int n) {
    return (n + 15) & ~15;
}

char *malloc(int size) {
    if (size <= 0) return (char *)0;

    int payload = align16(size);
    int needed = (int)sizeof(struct block_header) + payload;
    int start = align16(mt_heap_offset);
    if (start + needed > MT_HEAP_SIZE) return (char *)0;

    struct block_header *hdr = (struct block_header *)(mt_heap + start);
    hdr->size = payload;
    mt_heap_offset = start + needed;
    return (char *)(hdr + 1);
}

static void byte_copy(char *dst, const char *src, int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

char *realloc(char *ptr, int size) {
    if (!ptr) return malloc(size);
    if (size <= 0) return ptr;

    struct block_header *old_hdr = ((struct block_header *)ptr) - 1;
    int old_size = old_hdr->size;

    char *new_ptr = malloc(size);
    if (!new_ptr) return (char *)0;

    int copy_n = old_size < size ? old_size : size;
    byte_copy(new_ptr, ptr, copy_n);
    return new_ptr;
}
