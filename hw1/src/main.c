#include <stdio.h>
#include <stdlib.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

int main(int argc, char **argv)
{
    printf("Main: argc: %d, argv: %s\n",argc,*argv);
    if(validargs(argc, argv)){
        printf("Invalid\n");
        USAGE(*argv, EXIT_FAILURE);
    }
    if(global_options == HELP_OPTION){
        printf("Valid\n");
        USAGE(*argv, EXIT_SUCCESS);
    }
    // TO BE IMPLEMENTED
    printf("Status:%s, options: %ld\n", " Valid", global_options);
    return EXIT_FAILURE;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
