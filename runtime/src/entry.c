// Process entry: runs the program and flushes standard output.

#include "astra_rt.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;   // stored for astra_args_count/astra_arg when std.os arrives
    astra_main();
    astra_stdout_flush();
    return 0;
}
