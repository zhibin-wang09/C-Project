#include <stdlib.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the various options that were specified will be
 * encoded in the global variable 'global_options', where it will be
 * accessible elsewhere in the program.  For details of the required
 * encoding, see the assignment handout.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * @modifies global variable "global_options" to contain an encoded representation
 * of the selected program options.
 * @modifies global variable "diff_filename" to point to the name of the file
 * containing the diffs to be used.
 */

int stringLength(char* arg1){
    int count = 0;
    while(*(arg1 + count) != '\0'){
       count++;
    }
    return count;
}

int compareString(char *arg1, char *arg2){
    if(stringLength(arg1) != stringLength(arg2)){
        return -1;
    }
    int  count = 0;
    while(*(arg1 + count) != '\0'){
        if(*(arg1 + count) != *(arg2 + count)){
            return -1;
        }
        count++;
    }
    return 0;
}

int validDiff(char *filename){
    int filenameLength = stringLength(filename);
    if(filenameLength < 4) {
        return -1;
    }
    if(*(filename + filenameLength-1) != 't') return -1;
    if(*(filename + filenameLength-2) != 'x') return -1;
    if(*(filename + filenameLength-3) != 't') return -1;
    if(*(filename + filenameLength-4) != '.') return -1;
    return 0;

}

int validargs(int argc, char **argv) {
    // TO BE IMPLEMENTED
    //**argv is an array of strings.
    if(argc <= 1 ){
        return -1;
    }

    if(compareString(*(argv + 1),"-h") == 0){
        global_options = global_options | 0x01;
        return 0;
    }

    if(argc <= 4){
    //If -h is provided must be first and rest is ignored
    //Otherwise order does not matter.
    //Check for invalid argument i.e. argument other than -n -q -h

        for(int i = 1; i<argc;i++){
            char *s = *(argv + i);
            //Checks if -h is the first argument if it appears.
            if(i != 1 && compareString(s,"-h") == 0){
                return -1;
            }

            if(i!= argc-1){
                if(compareString(s,"-q") == 0){
                    global_options = global_options | 0x04;
                }else if(compareString(s,"-n") == 0){
                    global_options = global_options | 0x02;
                }else{
                    return -1;
                }
            }else{
                if(validDiff(s) == 0){
                    diff_filename =  s;
                }else{
                    return -1;
                }
            }

        }

        printf("%s\n","done");
        return 0;
    }

    return -1;
}

