
#include <stdlib.h>

extern int orig_main(int argc, char **argv);

int main(int argc, char **argv) {
    return orig_main(argc, argv);
}
