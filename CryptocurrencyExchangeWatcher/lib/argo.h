/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef ARGO_H
#define ARGO_H

/*
 * Definitions for "Argo" (aka JSON).
 */

#include <stdio.h>

/*
 * Opaque type representing an Argo value.
 * For simplicity, we do not expose the internals of the representation;
 * such a value must only be manipulated by the functions declared below. 
 */
typedef struct argo_value ARGO_VALUE;

/*
 * Functions exported by the "Argo API".
 */

/**
 * @brief  Parse an ARGO_VALUE from an input stream.
 *
 * @param  f  The input stream to read from.
 * @return  A valid pointer in case of success, NULL otherwise.
 */
ARGO_VALUE *argo_read_value(FILE *f);

/**
 * @brief  Unparse an ARGO_VALUE to an output stream.
 *
 * @param  vp  The ARGO_VALUE to output.
 * @param  f  The stream to output to.
 * @param  pretty  If nonzero, then format the output for human readability,
 * otherwise output all on one line with minimal whitespace.
 * @return  0 in case of success, nonzero otherwise.
 */
int argo_write_value(ARGO_VALUE *vp, FILE *f, int pretty);

/**
 * @brief  Free an ARGO_VALUE.
 */
void argo_free_value(ARGO_VALUE *);

/**
 * @brief Test whether an ARGO_VALUE is the basic value 'null'.
 *
 * @param vp  Pointer to the ARGO_VALUE.
 * @return nonzero if the value is indeed 'null', zero otherwise.
 */
int argo_value_is_null(ARGO_VALUE *vp);

/**
 * @brief  Test whether an ARGO_VALUE is the basic value 'true'.
 *
 * @param  vp  Pointer to the ARGO_VALUE.
 * @return nonzero if the value is indeed 'true', zero otherwise.
 */
int argo_value_is_true(ARGO_VALUE *vp);

/**
 * @brief  Test whether an ARGO_VALUE is the basic value 'false'.
 *
 * @param  vp  Pointer to the ARGO_VALUE.
 * @return nonzero if the value is indeed 'false', zero otherwise.
 */
int argo_value_is_false(ARGO_VALUE *vp);

/**
 * @brief  Test whether an ARGO_VALUE is a number, and obtain its
 * value as a long int, if it makes sense to do so.
 *
 * @param vp  Pointer to the ARGO_VALUE.
 * @param longp  Pointer to a variable in which to return the result.
 * @return 0 on success, -1 on error.
 */
int argo_value_get_long(ARGO_VALUE *vp, long *longp);

/**
 * @brief  Test whether an ARGO_VALUE is a number, and obtain its
 * value as a double, if it makes sense to do so.
 *
 * @param vp  Pointer to the ARGO_VALUE.
 * @param doublep  Pointer to a variable in which to return the result.
 * @return 0 on success, -1 on error.
 */
int argo_value_get_double(ARGO_VALUE *vp, double *doublep);

/**
 * @brief  Test whether an ARGO_VALUE is an ARGO_STRING and,
 * if it makes sense to do so, extract from it an ordinary
 * C string.  The caller must free the returned string.
 * Note that this only really makes sense if the ARGO_STRING
 * contains only ASCII characters.
 *
 * @param vp  Pointer to the ARGO_VALUE.
 * @return a valid pointer on success, NULL on error.
 */
char *argo_value_get_chars(ARGO_VALUE *vp);

/**
 * @brief  Test whether an ARGO_VALUE is an ARGO_OBJECT and,
 * if it makes sense to do so, extract from the member with
 * the specified name.  Note that only ASCII names are currently
 * supported.
 *
 * @param vp  Pointer to the Argo value.
 * @param name  Name of the member.
 * @return a valid pointer on success, NULL on error.
 */
ARGO_VALUE *argo_value_get_member(ARGO_VALUE *vp, char *name);

/**
 * @brief  Test whether an ARGO_VALUE is an ARGO_ARRAY and,
 * if it makes sense to do so, extract from the member with
 * the specified index.  Note that only ASCII names are currently
 * supported.
 *
 * @param vp  Pointer to the Argo value.
 * @param index  String representation of the index.
 * @return a valid pointer on success, NULL on error.
 */
ARGO_VALUE *argo_value_get_element(ARGO_VALUE *vp, char *index);

#endif

