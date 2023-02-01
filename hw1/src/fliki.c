#include <stdlib.h>
#include <stdio.h>

#include "fliki.h"
#include "global.h"
#include "debug.h"

long current_op;
int encountered_newline = 0; // Indicator of if we have see '\n'. Because header only exist after a '\n'. Default to 0 because start of file.
int serial = 0; //The current number of hunk.
int finish_hunk = 0; //Indicator of if hunk_next() is called. So hunk_getc() can not be called if hunk_next() is not called.

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

    char c;
    int oldstart;
    int oldend;
    int newstart;
    int newend;
    HUNK_TYPE op = HUNK_NO_TYPE;
    int seen_comma = -1; //Indicator for choosing to operate on old/new start or end.
    int comma_count = 0; //Count for duplicate commas.

    //Finding the beginning of a hunk, if's not already.
    //Requirement for a line to be the beginning of a hunk header:
        //Beginning of line
        //Start with number
    // If the *in pointed to the middle of a line that looks like a header, return ERR. <- wrong
    // *in can not possibly be pointing to the middle of line without the use of hunkgetc thus,
    // hunknext only need to worry about getting the next hunk using the above requirements. And hunk_getc() need to worry about if hunk_next() is called before itself.
    while((c=fgetc(in)) != EOF){
        if(encountered_newline == 0 && (c >= '0' || c <= '9')){
            ungetc(c,in); //Place the char back because we want the full content of header.
            break;
        }
        encountered_newline = c == '\n' ? 0 : 1;
    }

    if(c == EOF) return EOF;
    //Parse the header while checking for validity
    while((c=fgetc(in)) != '\n'){
        if((c <= '0' || c >= '9') && (c!= 'a' || c!='d' || c!='c' || c!=',')){
            return ERR; //If the header contains information that is not suppose to be there.
        }

        //Seen the comma
        if(c == ',') {
            seen_comma = 0;
            comma_count++;
        }

        //Check for duplicate ","
        if(comma_count > 1){
            return ERR;
        }

        if(op == HUNK_NO_TYPE){
            //Set the type
            if(c =='a'){
                op = HUNK_APPEND_TYPE;
                seen_comma = -1;
                //For addition, the old start/end can not have comma.
                if(comma_count >= 1){
                    return ERR;
                }
                comma_count = 0;
            }else if(c=='d') {
                op = HUNK_DELETE_TYPE; seen_comma = -1; comma_count =0;}
            else if (c=='c') {
                op = HUNK_CHANGE_TYPE; seen_comma = -1; comma_count = 0;}

            //Old line start and line end.
            if(!seen_comma){
                //Old line start
                oldstart *= 10;
                oldstart += (c - '0');
            }else{
                //Old line end
                oldend *= 10;
                oldend += (c-'0');
            }

        }else{
            //This block means we already have set an operation type.

            //Check for duplicate operations
            if(c =='a' || c =='d' || c=='c'){
                return ERR;
            }
            //For deletion the new start/end can not have comma
            if(op == HUNK_DELETE_TYPE && comma_count >= 1) {
                return ERR;
            }

            //New line start and line end.
            if(!seen_comma){
                //New line start
                newstart *= 10;
                newstart += (c - '0');
            }else{
                //New line end
                newend *= 10;
                newend += (c - '0');
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
    current_op = op; //Set the current state of operation for hunk_getc() to check to according errors.

    encountered_newline = -1; //Turn off encounter new line after reading the header.
    finish_hunk = -1; //Activate calls to hunk_getc()
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
    if(finish_hunk == 0) return ERR;
    abort();
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
    abort();
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
    // TO BE IMPLEMENTED
    abort();
}
