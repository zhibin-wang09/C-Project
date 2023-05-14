#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "ticker.h"

/*
 * These tests are very basic "blackbox" tests designed to mostly exercise
 * startup and shutdown of the program.
 */

Test(basecode_suite, startup_quit_test) {
    char *cmd = "echo quit | timeout -s KILL 5s bin/ticker";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

Test(basecode_suite, startup_EOF_test) {
    char *cmd = "cat /dev/null | timeout -s KILL 5s bin/ticker";
    int return_code = WEXITSTATUS(system(cmd));

    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

Test(basecode_suite, startup_watchers_test) {
    char *cmd = "(echo watchers; echo quit) | timeout -s KILL 5s bin/ticker > test_output/startup_watchers.out";
    char *cmp = "cmp test_output/startup_watchers.out tests/rsrc/startup_watchers.out";

    int return_code = WEXITSTATUS(system(cmd));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
    return_code = WEXITSTATUS(system(cmp));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program output did not match reference output.");
}
