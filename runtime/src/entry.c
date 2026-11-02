// Process entry: runs the program and flushes standard output.

#include "ofir_rt.h"

int main(int argc, char **argv) {
    int top;   // the main thread's stack starts about here
    ofir_stack_guard_init(&top);
    ofir_set_args(argc, argv);
    ofir_main();
    ofir_stdout_flush();
    return 0;
}
