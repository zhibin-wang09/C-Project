#include <stdlib.h>
#include <stdio.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

static int encountered_newline = 1; // Indicator of if we have see '\n'. Because header only exist after a '\n'. Default to 0 because start of file.
static int serial = 0; //The current number of hunk.
static int finish_hunk = 1; //Indicator of if hunk_next() is called. So hunk_getc() can not be called if hunk_next() is not called.
static int deletion_buffer_pos = 2; //Start at the third byte as the first 2 is for counting
static int addition_buffer_pos = 2; //Start at the third byte as the first 2 is for counting
static int newline_count = 0;
static int seen_threedash = 0; //Used to indicate if we see threedash in order to determine change type hunk
static int seen_deletion = 0;
static int seen_addition = 0;
static int current_deletion_buffer_marker; //Marker for the size of the line before this line
static int current_addition_buffer_marker;
static HUNK previous_hunk = {HUNK_NO_TYPE,0,0,0,0,0};
static int change_two_eos = 0; //Indicator for if a change hunk hit the first eos.
static int addition_line_size = 0; //The current line size of addition buffer
static int deletion_line_size = 0; //The current line size of deletion buffer
static int use_previous_hp = 0;
static int print_addition_hunk = 0;
static int call_from_next; //To diferentiate from using hunkgetc in hunknext as a helper function vs. calling hunk get c alone.
static int dead = 0;//Indicate if any error occured if it does then the program is dead.
static void update_deletion_buffer(int *size, int *pos, char c,int marker);
static void update_addition_buffer(int *size, int *pos, char c,int marker);
static int patch_qn(FILE *in, FILE *out, FILE *diff,long op);
static int patch_helper(FILE *in, FILE *out, FILE *diff, long global_options);

/**
 * @brief Get the header of the next hunk in a diff file.
 * @details This function advances to the beginning of the next hunk
 * in a diff file, reads and parses the header line of the hunk,
 * and initializes a HUNK structure with the result.
 *
 * @param hp  Pointer to a HUNK structure provided by the caller.
 * Information about the next hunk will be stored in this structure.
 * @param in  Input stream from which hunks are being read.
 * @return 0  if the next hunk was successfully located and parsed,
 * EOF if end-of-file was encountered or there was an error reading
 * from the input stream, or ERR if the data in the input stream
 * could not be properly interpreted as a hunk.
 */

int hunk_next(HUNK *hp, FILE *in) {
    // TO BE IMPLEMENTED
    if(in == NULL || hp == NULL){
        return EOF;
    }
    int i=0;
    while(i < deletion_buffer_pos) {
        *(hunk_deletions_buffer+(i++)) = 0;
        deletion_buffer_pos=2; //Reset deletion buffer pointer
        current_deletion_buffer_marker =0; //Reset marker
        deletion_line_size = 0; //Reset line size
    }
    i = 0;
    while(i < addition_buffer_pos){
        *(hunk_additions_buffer+(i++)) = 0;
        addition_buffer_pos = 2; //Reset addition buffer pointer.
        current_addition_buffer_marker = 0; //Reset marker
        addition_line_size = 0; //Reset line size
    }
    char c;
    newline_count = 0; //If we move on to the next hunk, we will need to reset the newline count.
    if(change_two_eos){
        if((c=fgetc(in))== '-'){
            // if(seen_deletion == 0) {
            //     return ERR;
            // }
            encountered_newline = 0;
            if((c=fgetc(in)) == '-'){
                if((c=fgetc(in)) == '-'){
                    char n = fgetc(in); //ignore new line
                    if(n != '\n'){return ERR;}
                    encountered_newline = 1;
                    seen_threedash = 1;  //End of the deletion section in change
                    change_two_eos = 0; //Allow to move on to addition section of change hunk.
                    use_previous_hp  =1; //Next call to hunk get c will use previous hp because the hunk header presists for the two sections
                    print_addition_hunk = 1;
                    newline_count = 0;
                    return 0; //Passes through the three dashes
                }else{
                    if(c == EOF) return EOF;
                    return ERR;
                }
            }else{
                if(c == EOF) return EOF;
                return ERR;
            }
        }
        return 0;
    }
    //clear both buffers
    if(finish_hunk == 1){
        previous_hunk = (HUNK){HUNK_NO_TYPE,0,0,0,0,0};
    }
    finish_hunk = 0;
    call_from_next = 1;
    while((c=hunk_getc((&previous_hunk),in)) != EOF && c != EOS){
        if(c == ERR){
            return ERR;
        }
        if(encountered_newline == 1 && (c >= '0' && c <= '9')){
            ungetc(c,in); //Place the char back because we want the full content of header.
            break;
        }
    }
    call_from_next = 0;
    if(c == EOF) return EOF;
    int oldstart = 0;
    int oldend = 0;
    int newstart =0;
    int newend = 0;
    HUNK_TYPE op = HUNK_NO_TYPE;
    int seen_comma = 0; //Indicator for choosing to operate on old/new start or end.
    int comma_count = 0; //Count for duplicate commas.
    int seen_integer = 0; //Indicator if we see an interger. Used to detect errors if no integer seen before comma.
    print_addition_hunk = 0;
    //int switch_side = 0; //Indicator to update the old start and end once the left side of the header is finish reading.
    seen_threedash = 0; //Instead of resseting three dashes in hunkgetc, we reset when we try to get the next hunk header. Because, we are certain that if we get next header we no longer seen threedashes.
    //Parse the header while checking for validity
    hp->serial = ++serial; //Update the serial number before we go in.
    while((c=fgetc(in)) != '\n'){
        if(c == -1){
            if(!encountered_newline) return ERR; //No new line at the end of the file is an error.
            return EOF;
        }
        if((c < '0' || c > '9') && c!= 'a' && c!='d' && c!='c' && c!=','){
            encountered_newline = 0; //Set to 0 so next run to hunk_next() will start at next new line.
            return ERR; //If the header contains information that is not suppose to be there.
        }

        //Seen the comma
        if(c == ',') {
            //Check if we see any integer before seeing comma
            if(!seen_integer) {
                encountered_newline = 0;
                return ERR;
            } //Error because we see comma before seeing integer.

            //Check if next one is a integer
            c = fgetc(in);
            if(c < '0' || c > '9'){
                encountered_newline = 0;
                return ERR;
            }
            ungetc(c,in);
            seen_comma = 1;
            comma_count++;
            continue; //Go to next char
        }

        //Check for duplicate ","
        if(comma_count > 1){
            encountered_newline = 0;
            return ERR;
        }

        if(op == HUNK_NO_TYPE){
            //Set the type
            if(c =='a'){
                op = HUNK_APPEND_TYPE;
                seen_comma = 0;
                seen_integer = 0;
                //For addition, the old start/end can not have comma.
                if(comma_count >= 1){
                    encountered_newline = 0;
                    return ERR;
                }
                comma_count = 0;
                continue;
            }else if(c=='d') {
                op = HUNK_DELETE_TYPE; seen_comma = 0; comma_count =0;continue;}
            else if (c=='c') {
                op = HUNK_CHANGE_TYPE; seen_comma = 0; comma_count = 0; seen_integer = 0;continue;}

            //Old line start and line end.
            if(!seen_comma){
                //Old line start
                oldstart *= 10;
                oldstart += (c - '0');
                seen_integer = 1;
            }else{
                //Old line end
                oldend *= 10;
                oldend += (c-'0');
                seen_integer = 1;
            }
        }else{
            //This block means we already have set an operation type.

            //Check for duplicate operations
            if(c =='a' || c =='d' || c=='c'){
                return ERR;
            }
            //For deletion the new start/end can not have comma
            if(op == HUNK_DELETE_TYPE && comma_count >= 1) {
                encountered_newline = 0;
                return ERR;
            }

            //New line start and line end.
            if(!seen_comma){
                //New line start
                newstart *= 10;
                newstart += (c - '0');
                seen_integer = 1;
            }else{
                //New line end
                newend *= 10;
                newend += (c - '0');
                seen_integer = 1;
            }
        }
    }
    serial++; //Finishing parsing hunk then we set it to serial++

    //In case old end or new end is the same as old start or new start.
    if(oldend == 0) oldend = oldstart;
    if(newend == 0) newend = newstart;

    //Build the HUNK
    hp->type = op;
    hp->serial =  serial;
    hp->old_start = oldstart;
    hp->old_end = oldend;
    hp->new_start = newstart;
    hp->new_end = newend;

    previous_hunk = (HUNK){(*hp).type, (*hp).serial, (*hp).old_start, (*hp).old_end,(*hp).new_start,(*hp).new_end}; //Clone previious hp

    encountered_newline = 1; //Finished header after reading a new line
    finish_hunk = 0;
    return 0;
}

/**
 * @brief  Get the next character from the data portion of the hunk.
 * @details  This function gets the next character from the data
 * portion of a hunk.  The data portion of a hunk consists of one
 * or both of a deletions section and an additions section,
 * depending on the hunk type (delete, append, or change).
 * Within each section is a series of lines that begin either with
 * the character sequence "< " (for deletions), or "> " (for additions).
 * For a change hunk, which has both a deletions section and an
 * additions section, the two sections are separated by a single
 * line containing the three-character sequence "---".
 * This function returns only characters that are actually part of
 * the lines to be deleted or added; characters from the special
 * sequences "< ", "> ", and "---\n" are not returned.
 * @param hdr  Data structure containing the header of the current
 * hunk.
 *
 * @param in  The stream from which hunks are being read.
 * @return  A character that is the next character in the current
 * line of the deletions section or additions section, unless the
 * end of the section has been reached, in which case the special
 * value EOS is returned.  If the hunk is ill-formed; for example,
 * if it contains a line that is not terminated by a newline character,
 * or if end-of-file is reached in the middle of the hunk, or a hunk
 * of change type is missing an additions section, then the special
 * value ERR (error) is returned.  The value ERR will also be returned
 * if this function is called after the current hunk has been completely
 * read, unless an intervening call to hunk_next() has been made to
 * advance to the next hunk in the input.  Once ERR has been returned,
 * then further calls to this function will continue to return ERR,
 * until a successful call to call to hunk_next() has successfully
 * advanced to the next hunk.
 */

int hunk_getc(HUNK *hp, FILE *in) {
    // TO BE IMPLEMENTED
    //finished a hunk or finished a secition in change hunk
    if(finish_hunk == 1) return ERR;
    if(change_two_eos) return EOS;
    if(hp == NULL || in == NULL) return ERR;
    //Ignore "> " , "< " ,"\n"
    if(use_previous_hp){
        *hp = previous_hunk; //hp will be using previous hunk which is a change hunk.
        use_previous_hp =0;
    }
    char c;
    while((c = fgetc(in)) != EOF){
        //Next hunk header consist of after new line and is a number.
        //Although there is a chance of erro where next new line start with 3a3 and
        //the hunk is not finished but that would be header's error.
        if(encountered_newline && c >= '0' && c <='9'){
            ungetc(c,in); //put the number back so header can parse the correct information
            // if((*hp).type == 1 && !seen_addition){
            //     return ERR; //Addition hunk missing addition section.
            // }
            // if((*hp).type == 2 && !seen_deletion){
            //     return ERR; //Deletion hunk missing deletion section.
            // }
            if((*hp).type == 3 &&  (!seen_threedash /*|| !seen_addition || !seen_deletion*/)) {
                return ERR; //Change hunk missing either three dashes, deletion section, or addition section
            }
            finish_hunk = 1;
            //Reset all the counting information
            //seen_threedash = 0; delete this because for change hunk, at the end of the addition section we don't want to reset seen three dashes because we could have other tasks
            seen_addition = 0;
            seen_deletion = 0;
            encountered_newline = 1;
            return EOS;
        }

        if((*hp).type == 1){ //It's a addition hunk

            //Check if ">" is followed by a new line.
            if(encountered_newline){
                if(c == '>'){ //Found '>' after a new line
                    //Check for ">" followed by a space.
                    if((c = fgetc(in)) != ' '){
                        return ERR;
                    }
                    c = fgetc(in);
                    seen_addition = 1;


                }else{ //Found something else
                    encountered_newline = 0; //Reset encountered_newline to have next call to hunknext or getc ignore current line.
                    return ERR;
                }
            }

        }else if((*hp).type == 2){ //It'a deletion hunk
            //Check if '<' is followed by a new line
            if(encountered_newline){
                if(c == '<'){//Found '<'
                    //Check if '<' is followed by a space
                    if((c = fgetc(in)) != ' '){
                        return ERR;
                    }
                    c = fgetc(in);
                    seen_deletion = 1;
                }else{ //Foudn something that's not '<'
                    encountered_newline = 0;
                    return ERR;
                }
            }
        }else if((*hp).type == 3){ //It's a change hunk
            //Deletion comes before addition
            //Deletion
            //---
            //Addition

            //NEED TO CHECK FOR IF ADDITION SECTION MISSING.
            if(encountered_newline){
                if(c == '-'){
                    if(change_two_eos == 0 && !call_from_next){
                        ungetc(c,in);
                        change_two_eos = 1;
                        call_from_next=0;
                        previous_hunk = (HUNK){(*hp).type, (*hp).serial, (*hp).old_start, (*hp).old_end,(*hp).new_start,(*hp).new_end}; //Clone previious hp
                        return EOS;
                    }
                    // if(seen_deletion == 0) {
                    //     return ERR;
                    // }
                    encountered_newline = 0;
                    if((c=fgetc(in)) == '-'){
                        if((c=fgetc(in)) == '-'){
                            char n = fgetc(in); //ignore new line
                            if(n != '\n'){return ERR;}
                            encountered_newline = 1;
                            seen_threedash = 1;
                            continue; //End of the deletion section in change
                        }else{
                            if(c == EOF) return EOF;
                            return ERR;
                        }
                    }else{
                        if(c == EOF) return EOF;
                        return ERR;
                    }
                }
            }

            if(!seen_threedash){

                if(encountered_newline){ //Deletion hunk
                    if(c == '<'){
                        if((c=fgetc(in)) != ' '){
                            return ERR;
                        }
                        c= fgetc(in); //Ignore space
                        seen_deletion = 1;
                    }else{
                        encountered_newline = 0;
                        return ERR;
                    }

                }
            }else{
                if(encountered_newline){ //addition hunk
                    if(c == '>'){
                        if((c=fgetc(in)) != ' '){
                            return ERR;
                        }
                        c= fgetc(in); //Ignore space
                        seen_addition = 1;
                    }else{
                        encountered_newline = 0;
                        return ERR;
                    }
                }
            }
        }
        if(!call_from_next){
            if((*hp).type == 1){
                update_addition_buffer(&addition_buffer_pos, &addition_line_size, c, current_addition_buffer_marker); //Store char into buffer
            }else if((*hp).type == 2){
                update_deletion_buffer(&deletion_buffer_pos, &deletion_line_size, c, current_deletion_buffer_marker); //Store char into buffer
            }else if((*hp).type == 3){
                if(seen_deletion && !seen_threedash){
                    update_deletion_buffer(&deletion_buffer_pos, &deletion_line_size,c, current_deletion_buffer_marker); //Store char into buffer
                }
                if(seen_addition){
                    update_addition_buffer(&addition_buffer_pos, &addition_line_size,c, current_addition_buffer_marker); //Store char into buffer
                }
            }
        }


        //For change section deletion comes before addition.
        encountered_newline = c == '\n' ? 1 : 0;
        if(encountered_newline){
            newline_count++; //increment newline_count
            //check which hunk_buffer we are using right now
            if(!call_from_next){
                if((*hp).type == 1){
                    addition_line_size = 0;
                    current_addition_buffer_marker = addition_buffer_pos;
                    addition_buffer_pos+=2;
                }else if((*hp).type == 2){
                    deletion_line_size = 0;
                    current_deletion_buffer_marker = deletion_buffer_pos;
                    deletion_buffer_pos+=2;
                }else if((*hp).type == 3){
                    if(seen_deletion && !seen_threedash){
                        deletion_line_size = 0;
                        current_deletion_buffer_marker = deletion_buffer_pos;
                        deletion_buffer_pos +=2;
                    }
                    if(seen_addition){
                        addition_line_size =0;
                        current_addition_buffer_marker = addition_buffer_pos;
                        addition_buffer_pos+=2;
                    }
                }
            }
        }
        return c; //Return the character read
    }
    // if((*hp).type == 1 && !seen_addition){
    //     return ERR; //Addition hunk missing addition section.
    // }
    // if((*hp).type == 2 && !seen_deletion){
    //     return ERR; //Deletion hunk missing deletion section
    // }
    // if((*hp).type == 3 &&  (!seen_threedash || !seen_addition || !seen_deletion)) {
    //     return ERR; //Change hunk missing either three dashes, deletion section, or addition section
    // }
    if(!encountered_newline) {return ERR;} //EOF in the middle of the hunk.
    finish_hunk =1 ;
    return EOS;
}

/**
 * @brief  Print a hunk to an output stream.
 * @details  This function prints a representation of a hunk to a
 * specified output stream.  The printed representation will always
 * have an initial line that specifies the type of the hunk and
 * the line numbers in the "old" and "new" versions of the file,
 * in the same format as it would appear in a traditional diff file.
 * The printed representation may also include portions of the
 * lines to be deleted and/or inserted by this hunk, to the extent
 * that they are available.  This information is defined to be
 * available if the hunk is the current hunk, which has been completely
 * read, and a call to hunk_next() has not yet been made to advance
 * to the next hunk.  In this case, the lines to be printed will
 * be those that have been stored in the hunk_deletions_buffer
 * and hunk_additions_buffer array.  If there is no current hunk,
 * or the current hunk has not yet been completely read, then no
 * deletions or additions information will be printed.
 * If the lines stored in the hunk_deletions_buffer or
 * hunk_additions_buffer array were truncated due to there having
 * been more data than would fit in the buffer, then this function
 * will print an elipsis "..." followed by a single newline character
 * after any such truncated lines, as an indication that truncation
 * has occurred.
 *
 * @param hp  Data structure giving the header information about the
 * hunk to be printed.
 * @param out  Output stream to which the hunk should be printed.
 */

void hunk_show(HUNK *hp, FILE *out) {
    // TO BE IMPLEMENTED
    if(hp == NULL) return;
    int new_start = (*hp).new_start;
    int new_end = (*hp).new_end;
    int old_start = (*hp).old_start;
    int old_end = (*hp).old_end;
    //Prints header
    if((*hp).type == 1){
        if(new_start != new_end) fprintf(out, "%da%d,%d\n",old_start, new_start,new_end); //multiple line for new
        else fprintf(out, "%da%d\n",old_start, new_start); //Single line add
    }else if((*hp).type == 2){
        if(old_start != old_end) fprintf(out, "%d,%dd%d\n",old_start, old_end,new_start); //Multiple line for new
        else fprintf(out, "%dd%d\n",(*hp).old_start, new_start); //Single line delete
    }else if((*hp).type == 3){
        if(new_start != new_end){
            if(old_start != old_end) fprintf(out, "%d,%dc%d,%d\n",old_start,old_end,new_start,new_end); //Multiple line change
            else fprintf(out, "%dc%d,%d\n",old_start, new_start,new_end); //Multiple new line change
        }else{
            if(old_start != old_end) fprintf(out, "%d,%dc%d\n",old_start, old_end,new_start); //Multiple old line change
            else fprintf(out, "%dc%d\n",old_start, new_start); //Single line change
        }
    }else{
        return;
    }
    if((*hp).type == 3 && change_two_eos){finish_hunk = 1;} //finishhunk edge case, where the we will need to indicate the half section.
    if(!finish_hunk){return; }//If did not finish hunk, then we will not print the body.
    if((*hp).type == 3 && change_two_eos){finish_hunk = 0;}
    //Finished hunk we completly read the hunk then we can print out.
    if(*(hunk_additions_buffer+HUNK_MAX-3) != 0){
        *(hunk_additions_buffer+HUNK_MAX-4) = '\n';
        *(hunk_additions_buffer+HUNK_MAX-5) = '.';
        *(hunk_additions_buffer+HUNK_MAX-6) = '.';
        *(hunk_additions_buffer+HUNK_MAX-7) = '.';
    }
    if(*(hunk_deletions_buffer+HUNK_MAX-3) != 0){
        *(hunk_deletions_buffer+HUNK_MAX-4) = '\n';
        *(hunk_deletions_buffer+HUNK_MAX-5) = '.';
        *(hunk_deletions_buffer+HUNK_MAX-6) = '.';
        *(hunk_deletions_buffer+HUNK_MAX-7) = '.';
    }
    //Print the hunk data section
    int i = 2;
    char c;
    int printsymbol = 1;
    if((*hp).type == 1){
        while(i< addition_buffer_pos-2){
            c = *(hunk_additions_buffer+i);
            if(c != '\n'){
                if(printsymbol){
                    fprintf(out, "> ");
                    printsymbol =0;
                }
                fprintf(out, "%c",c);
            }else{
                fprintf(out,"%c",c);
                printsymbol = 1;
                i+=2;
            }
            i++;
        }
    }else if((*hp).type ==2){
        while(i< deletion_buffer_pos-2){
            c = *(hunk_deletions_buffer+i);
            if(c != '\n'){
                if(printsymbol){
                    fprintf(out,"< ");
                    printsymbol = 0;
                }
                fprintf(out,"%c",c);
            }else{
                fprintf(out,"%c",c);
                printsymbol =1;
                i+=2;
            }
            i++;
        }
    }else if((*hp).type == 3){
        if(print_addition_hunk){
            while(i< addition_buffer_pos-2){
                c = *(hunk_additions_buffer+i);
                if(c != '\n'){
                    if(printsymbol){
                        fprintf(out,"> ");
                        printsymbol =0;
                    }
                    fprintf(out,"%c",c);
                }else{
                    fprintf(out,"%c",c);
                    printsymbol=1;
                    i+=2;
                }
                i++;
            }
        }else{
            while(i< deletion_buffer_pos-2){
                c = *(hunk_deletions_buffer+i);
                if(c != '\n'){
                    if(printsymbol){
                        fprintf(out,"< ");
                        printsymbol =0;
                    }
                    fprintf(out,"%c",c);
                }else{
                    fprintf(out,"%c",c);
                    printsymbol =1;
                    i+=2;
                }
                i++;
            }
        }
    }
}

/**
 * @brief  Patch a file as specified by a diff.
 * @details  This function reads a diff file from an input stream
 * and uses the information in it to transform a source file, read on
 * another input stream into a target file, which is written to an
 * output stream.  The transformation is performed "on-the-fly"
 * as the input is read, without storing either it or the diff file
 * in memory, and errors are reported as soon as they are detected.
 * This mode of operation implies that in general when an error is
 * detected, some amount of output might already have been produced.
 * In case of a fatal error, processing may terminate prematurely,
 * having produced only a truncated version of the result.
 * In case the diff file is empty, then the output should be an
 * unchanged copy of the input.
 *
 * This function checks for the following kinds of errors: ill-formed
 * diff file, failure of lines being deleted from the input to match
 * the corresponding deletion lines in the diff file, failure of the
 * line numbers specified in each "hunk" of the diff to match the line
 * numbers in the old and new versions of the file, and input/output
 * errors while reading the input or writing the output.  When any
 * error is detected, a report of the error is printed to stderr.
 * The error message will consist of a single line of text that describes
 * what went wrong, possibly followed by a representation of the current
 * hunk from the diff file, if the error pertains to that hunk or its
 * application to the input file.  If the "quiet mode" program option
 * has been specified, then the printing of error messages will be
 * suppressed.  This function returns immediately after issuing an
 * error report.
 *
 * The meaning of the old and new line numbers in a diff file is slightly
 * confusing.  The starting line number in the "old" file is the number
 * of the first affected line in case of a deletion or change hunk,
 * but it is the number of the line *preceding* the addition in case of
 * an addition hunk.  The starting line number in the "new" file is
 * the number of the first affected line in case of an addition or change
 * hunk, but it is the number of the line *preceding* the deletion in
 * case of a deletion hunk.
 *
 * @param in  Input stream from which the file to be patched is read.
 * @param out Output stream to which the patched file is to be written.
 * @param diff  Input stream from which the diff file is to be read.
 * @return 0 in case processing completes without any errors, and -1
 * if there were errors.  If no error is reported, then it is guaranteed
 * that the output is complete and correct.  If an error is reported,
 * then the output may be incomplete or incorrect.
 */

int patch(FILE *in, FILE *out, FILE *diff) {
    //Step 1: Read hunk header
    //Step 2: Store in hunk
    //Step 3: Read the char in diff file write it to out
    //Step 4: Read the char in in file write it to out
    // TO BE IMPLEMENTED
    if(in==NULL || out==NULL || diff==NULL) return -1;
    char c; //return value for hunkgetc
    if((c=fgetc(diff)) == EOF){ //diff file is enpty
        while((c= fgetc(in)) != EOF){ //Write the in file to out file unchanged.
            fprintf(out,"%c",c); //Write to out
        }
        return 0;
    }
    ungetc(c,diff); //Put the char back after testing
    int status; //status of hunknext
    HUNK header = {HUNK_NO_TYPE,0,0,0,0,0};
    if(global_options == NO_PATCH_OPTION){ // -n -> does not write anything to out. Used for testing.
        int status = patch_qn(in, out, diff, global_options);
        return status;
    }else if(global_options == QUIET_OPTION){ //-q
        int status = patch_helper(in,out,diff,global_options);
        return status;
    }else if(global_options == 6){ // -n -q
        int status = patch_qn(in, out, diff, global_options);
        return status;
    }else{ //No other options means print to out
        int status = patch_helper(in,out,diff,global_options);
        return status;
    }

    return 0;
}


void update_deletion_buffer(int *pos, int *length, char c,int marker){
    if(*pos >= HUNK_MAX-2) return;
    *(hunk_deletions_buffer + (*pos )) = c; //Store the char
    (*pos)++;
    (*length)++;
    unsigned int left_half = (*length) & 255; //Mask lower 8 bits = 1 byte
    unsigned int right_half = (unsigned int) (*length) >> 8; //Shift down 8 bits
    right_half = right_half & 255; //Mask lower 8 bits
    *(hunk_deletions_buffer + marker) = left_half;
    *(hunk_deletions_buffer + marker+1) = right_half;
}

void update_addition_buffer(int *pos, int *length, char c,int marker){
    if(*pos >= HUNK_MAX-2) return;
    *(hunk_additions_buffer + (*pos)) = c; //Store the char
    (*pos)++;
    (*length)++;
    unsigned int left_half = (*length) & 255; //Mask lower 8 bits = 1 byte
    unsigned int right_half = (unsigned int) (*length) >> 8; //Shift down 8 bits
    right_half = right_half & 255; //Mask lower 8 bits
    //printf("%d,%d, ",left_half,right_half);
    *(hunk_additions_buffer + marker) = left_half;
    *(hunk_additions_buffer + marker+1) = right_half;
    //printf("left: %d, right: %u",*(hunk_additions_buffer+marker),*(hunk_additions_buffer+marker+1));
}

/*A helper function that factors the similiar operation for no_patch_mode and no_patch_mode & quiet_mode*/
int patch_qn(FILE *in, FILE *out, FILE *diff,long op){
    int status; //status of hunknext
    HUNK header = {HUNK_NO_TYPE,0,0,0,0,0};
    char c;
    int in_line_count = 1;
    char in_c;
    while((status=hunk_next(&header,diff)) != EOF){ //parse the header.
        if(status == ERR){
            if(op == 2){fprintf(stderr, "Error, hunk header is invalid\n");hunk_show(&header, stderr);} //produce the error result
            return -1;
        }
        //Also move along the in file because we want to check if the deletion sections match.
        //NOTE: If there is a gap between current line in in file and current hunk start line. Write all the line
        //in between the in file to out file.
        while(in_line_count < header.old_start){
            //Write lines in between to out.
            in_c = fgetc(in); //get char in file in
            if(in_c == '\n'){in_line_count++;} //We reached a new line.
        }

        while((c = hunk_getc(&header,diff)) != EOS){ // get the char
            if(c == ERR){ //If some err occured return.
                if(op == 2){fprintf(stderr, "Error, hunk data section format is wrong\n");hunk_show(&header, stderr);} //produce the error result
                return -1;
            }
            /*if this section is a deletion hunk or deletion section of a change hunk,
            we need to check if the sections match by looking at file in and diff at the same time.*/
            if(header.type == 2 || (header.type == 3 && seen_deletion == 1 && !seen_threedash)){
                in_c = fgetc(in);
                if(c != in_c){
                    if(op == 2){fprintf(stderr,"Error, hunk deletion section don't match with input file\n");hunk_show(&header,stderr);}
                    return -1;
                } //deletion sections don't match
                if(in_c == '\n'){in_line_count++;}
            }
        }
        //printf("hunk type: %d, serial: %d, old start: %d, old end: %d,new start: %d, new end: %d\n", header.type, header.serial, header.old_start, header.old_end, header.new_start, header.new_end);
        //Compare if the lines are correct in the data section match the header section.
        if((header).type == 1){
            int lines_in_hunk = (header).new_end - (header).new_start + 1; //Inclusive so + 1
            if(lines_in_hunk != newline_count) {

                if(op == 2){fprintf(out,"Error, lines don't match\n");hunk_show(&header, stderr);}//produce the error result
                return -1;
            }
        }else if((header).type == 2){
            int lines_in_hunk = (header).old_end - (header).old_start + 1; //Inclusive so + 1
            if(lines_in_hunk != newline_count) {
                if(op == 2){fprintf(stderr,"Error, lines don't match\n");hunk_show(&header, stderr);} //produce the error result
                return -1;
            }
        }else if((header).type == 3){
            if(!seen_threedash){
                //Still in deletion section
                int lines_in_hunk = (header).old_end - (header).old_start + 1; //Inclusive so + 1
                if(lines_in_hunk != newline_count) {
                    if(op == 2){fprintf(stderr,"Error, lines don't match\n");hunk_show(&header, stderr);} //produce the error result
                    return -1;
                }
            }else{
                //In the addition section
                int lines_in_hunk = (header).new_end - (header).new_start + 1; //Inclusive so + 1
                if(lines_in_hunk != newline_count) {
                    if(op == 2){fprintf(stderr,"Error, lines don't match\n");hunk_show(&header, stderr);} //produce the error result}
                    return -1;
                }
            }
        }
        //Remeber to check if the change sections, if we just passed deletion, we should not refresh.
        header = (HUNK){HUNK_NO_TYPE,0,0,0,0,0};
    }
    return 0;
}

/*A function that factors similiar operation for patch mode and patch mode & quiet_mode*/
int patch_helper(FILE *in, FILE *out, FILE *diff, long op){
    int status;
    HUNK header = {HUNK_NO_TYPE,0,0,0,0,0};
    char diff_c;
    char in_c;
    int in_line_count = 1; //We are always in the first line in file in

    while((status = hunk_next(&header, diff))){
        if(status == ERR){
            if(status == ERR) return -1;
            fprintf(stderr, "Error, hunk header is invalid\n");
            return -1;
        }
        //Valid hunk header
        //NOTE: If there is a gap between current line in in file and current hunk start line. Write all the line
        //in between the in file to out file.
        while(in_line_count < header.old_start){
            //Write lines in between to out.
            in_c = fgetc(in); //get char in file in
            fputc(in_c,out); //write the char to file out
            if(in_c == '\n'){in_line_count++;} // We reached a new line.
        }

        while((diff_c = hunk_getc(&header, diff)) != EOS){
            //Change hunk -> For change hunk, read through left side X,X and check against in file.(Do not write this to output)
            //-> changehunk, read through the right side, and write these to the output.
            if(diff_c == ERR){return -1;}
            if(header.type == 3){

                //Compare the content and check if it matches
                if(seen_deletion && !seen_threedash){
                    in_c = fgetc(in);
                    if(c != in_c){
                        if(op == 2){fprintf(stderr,"Error, hunk deletion section don't match with input file\n");hunk_show(&header,stderr);}
                        return -1;
                    } //deletion sections don't match
                    if(in_c == '\n'){in_line_count++;}
                }
            }else if(header.type == 2){
                //Deletion hunk -> Ignore the lines that are inside deletion hunk. (i.e. do not write to out)
                //Compare the content and check if it matches
                in_c = fgetc(in);
                if(c != in_c){
                    if(op == 2){fprintf(stderr,"Error, hunk deletion section don't match with input file\n");hunk_show(&header,stderr);}
                    return -1;
                } //deletion sections don't match
                if(in_c == '\n'){in_line_count++;}
            }else if(header.type == 1){
                 //Additoin hunk -> read the body and write it to the out. At the line of left side of middle of the header.
                fputc(diff_c , out); //Write the chars read to out.
                if(diff_c == '\n'){in_line_count++;} //increment the inlinecount as we have finished writing a line.
            }
        }

        //Check if the lines in the diff file is valid. (Semantic errors)
        if((header).type == 1){
            int lines_in_hunk = (header).new_end - (header).new_start + 1; //Inclusive so + 1
            if(lines_in_hunk != newline_count) {
                if(op == 0){fprintf(out,"Error, lines don't match\n");hunk_show(&header, stderr);}//produce the error result
                return -1;
            }
        }else if((header).type == 2){
            int lines_in_hunk = (header).old_end - (header).old_start + 1; //Inclusive so + 1
            if(lines_in_hunk != newline_count) {
                if(op == 0){fprintf(stderr,"Error, lines don't match\n");hunk_show(&header, stderr);} //produce the error result
                return -1;
            }
        }else if((header).type == 3){
            if(!seen_threedash){
                //Still in deletion section
                int lines_in_hunk = (header).old_end - (header).old_start + 1; //Inclusive so + 1
                if(lines_in_hunk != newline_count) {
                    if(op == 0){fprintf(stderr,"Error, lines don't match\n");hunk_show(&header, stderr);} //produce the error result
                    return -1;
                }
            }else{
                //In the addition section
                int lines_in_hunk = (header).new_end - (header).new_start + 1; //Inclusive so + 1
                if(lines_in_hunk != newline_count) {
                    if(op == 0){fprintf(stderr,"Error, lines don't match\n");hunk_show(&header, stderr);} //produce the error result}
                    return -1;
                }
            }
        }
    }
    return 0;
}