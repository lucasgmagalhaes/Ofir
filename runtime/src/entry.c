// Process entry: runs the program and flushes standard output.

#include "astra_rt.h"

int main(int argc, char **argv) {
    int top;   // the main thread's stack starts about here
    astra_stack_guard_init(&top);
    astra_set_args(argc, argv);
    astra_main();
    astra_stdout_flush();
    return 0;
}
