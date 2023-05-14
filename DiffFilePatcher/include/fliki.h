/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef REVERKI_H
#define REVERKI_H

/*
 * Data structure definitions for "Fliki".
 * These are referenced by the interface specifications for the
 * functions you are to implement.
 */

/*
 * The following enum specifies the types of edit commands
 * (append, delete, or change) that can appear in a traditional
 * 'diff' file.  The HUNK_NO_TYPE constant (value = 0) does not
 * specify any type of edit command.  I typically leave value 0
 * unassigned in an enum so that there is an out-of-band value
 * to use as a sentinel or error return value, should this be
 * necessary.
 */
typedef enum {
    HUNK_NO_TYPE,
    HUNK_APPEND_TYPE,
    HUNK_DELETE_TYPE,
    HUNK_CHANGE_TYPE
} HUNK_TYPE;

/*
 * The following structure records summary information present in
 * a single "hunk" of a diff file.  It does not include the lists
 * of data lines to be deleted or appended by the hunk.  This is
 * because "fliki" performs its editing operation "on-the-fly",
 * reading the diff file and the file to be edited at the same time,
 * without buffering an entire hunk (or even an entire line of data)
 * in memory at any time.  The only exception to this is that the
 * hunk_deletions_buffer and hunk_additions_buffer may store a
 * (potentially truncated) version of the additions and deletions
 * lines, for the purpose of producing more specific reports in case
 * of an error.
 */
typedef struct hunk {
    HUNK_TYPE type;  // Type of edit command represented by the hunk.
    int serial;      // Serial number (starting from 1) of this hunk.
    int old_start;   // Starting line number in the old (original) file.
    int old_end;     // Ending line number in the old (original) file.
    int new_start;   // Starting line number in the new (patched) file.
    int new_end;     // Ending line number in the new (patched) file.
} HUNK;

#endif
