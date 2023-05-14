/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>

#include "fliki.h"

/*
 * USAGE macro to be called from main() to print a help message and exit
 * with a specified exit status.
 */
#define USAGE(program_name, retcode) do { \
fprintf(stderr, "USAGE: %s %s\n", program_name, \
"[-h] [-n] [-q] DIFF_FILE\n" \
"   -h       Help: displays this help menu.\n" \
"   -n       Dry run: no patched output is produced.\n" \
"   -q       Quiet mode: no error output is produced.\n" \
"\n" \
"If -h is specified, then it must be the first option on the command line, and any\n"\
"other options are ignored.\n" \
"\n" \
"If -h is not specified, then a filename DIFF_FILE must be the last argument.  In this case,\n" \
"the program reads the DIFF_FILE, which is assumed to be in the traditional format\n" \
"produced by 'diff' and it applies the edits specified therein to lines of text read\n" \
"from stdin.  The patched result is written to stdout and any error reports are written\n" \
"to stderr.\n" \
"\n" \
"If the program succeeds in applying all the patches without any errors, then it exits\n" \
"with exit code EXIT_SUCCESS.  Otherwise, the program exits with EXIT_FAILURE after issuing\n" \
"an error report on stderr.  In case of an error, the output that is produced on stdout may\n" \
"be truncated at the point at which the error was detected.\n" \
"\n" \
"If the -n option is specified, then the program performs a 'dry run' in which only error\n" \
"reports are produced (on stderr) and no patched output is produced on stdout.\n" \
"\n" \
"If the -q option is specified, then the program does not produce any error reports on\n" \
"stderr, although it still exits with EXIT_FAILURE should an error occur.\n" \
"\n" \
); \
exit(retcode); \
} while(0)

/*
 * Options info, set by validargs.
 *   If -h is specified, then the HELP_OPTION bit is set.
 */
long global_options;

/*
 * Name of the file containing the diff to be used.
 */
char *diff_filename;

/*
 * Bits that are OR-ed in to global_options to specify various modes of
 * operation.
 */
#define HELP_OPTION      (0x00000001)
#define NO_PATCH_OPTION  (0x00000002)
#define QUIET_OPTION     (0x00000004)

/*
 * Error codes that are specified to be returned by various functions
 * in particular situations.  These are in addition to EOF, which is
 * already defined as (-1) in <stdio.h>.
 */
#define ERR (-2)
#define EOS (-3)

/*
 * The following arrays provide storage for an initial portion of
 * the deletion and addition lines for the current "hunk".  This storage
 * will in general be insufficient to hold the complete data for a hunk.
 * It is to be used only for the purpose of printing a recognizable
 * representation of a hunk in case that hunk fails to apply to the input;
 * it is not used in the process of actually applying a hunk, and therefore
 * whether or not the full set of addition and/or deletion lines can
 * be stored only affects error reports output to stderr, and not the
 * patched result output to stdout.  These buffers are to be populated
 * "on-the-fly" as each hunk is read, so that once a complete hunk has been
 * read the buffered data is available for use in printing out a representation
 * of the hunk in an error message.
 *
 * The hunk_deletions_buffer array is used to an initial portion of the
 * deletions section of a hunk.
 * The hunk_additions_buffer array is used to store an initial portion of
 * the additions section of a hunk.
 *
 * Refer to the assignment handout for details on how the addition or
 * deletion lines are to be stored in each buffer.
 */

#define HUNK_MAX 512
char hunk_deletions_buffer[HUNK_MAX];
char hunk_additions_buffer[HUNK_MAX];

/*
 * Function you are to implement that validates and interprets command-line arguments
 * to the program.  See the stub in validargs.c for specifications.
 */
extern int validargs(int argc, char **argv);

/*
 * Functions you are to implement that perform the main functions of the program.
 * See the assignment handout and the comments in front of the stub for each function
 * in fliki.c for full specifications.
 */
extern int patch(FILE *in, FILE *out, FILE *diff);

extern int hunk_next(HUNK *hp, FILE *in);
extern int hunk_getc(HUNK *hp, FILE *in);
extern void hunk_show(HUNK *hp, FILE *out);

#endif
