// Process entry: runs the program and flushes standard output.

#include "astra_rt.h"

int main(int argc, char **argv) {
    astra_set_args(argc, argv);
    astra_main();
    astra_stdout_flush();
    return 0;
}
