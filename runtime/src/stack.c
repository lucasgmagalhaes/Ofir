// Stack overflow detection (docs/design/runtime.md, 3): a SIGSEGV handler,
// on an alternate stack, that reports a fault near the end of the main
// thread's stack as a panic instead of an anonymous crash.

#define _XOPEN_SOURCE 700

#include "ofir_rt.h"

#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#define ALT_STACK_SIZE (64 * 1024)
#define SLACK (1024 * 1024)               // below the limit: the guard area and big frames
#define DEFAULT_LIMIT (8L * 1024 * 1024)  // when the limit is unlimited

static char alt_stack[ALT_STACK_SIZE];
static uintptr_t stack_top, stack_bottom;

static void on_fault(int sig, siginfo_t *info, void *context) {
    (void)context;
    uintptr_t addr = (uintptr_t)info->si_addr;
    if (addr >= stack_bottom && addr < stack_top) {
        static const char msg[] = "panic: stack overflow\n";
        ofir_stdout_flush();
        ssize_t ignored = write(2, msg, sizeof msg - 1);
        (void)ignored;
        _exit(OFIR_PANIC_STATUS);
    }
    signal(sig, SIG_DFL);   // another fault: returning re-raises it unchanged
}

void ofir_stack_guard_init(void *top) {
    struct rlimit limit;
    long size = DEFAULT_LIMIT;
    if (getrlimit(RLIMIT_STACK, &limit) == 0 && limit.rlim_cur != RLIM_INFINITY &&
        limit.rlim_cur < (rlim_t)(1L << 30)) {
        size = (long)limit.rlim_cur;
    }
    stack_top = (uintptr_t)top;
    stack_bottom = stack_top > (uintptr_t)(size + SLACK) ? stack_top - (uintptr_t)(size + SLACK)
                                                         : 0;
    stack_t alt = {.ss_sp = alt_stack, .ss_size = sizeof alt_stack, .ss_flags = 0};
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_sigaction = on_fault;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    if (sigaltstack(&alt, NULL) == 0) {
        sigaction(SIGSEGV, &action, NULL);
        sigaction(SIGBUS, &action, NULL);   // macOS reports some overflows as SIGBUS
    }
}
