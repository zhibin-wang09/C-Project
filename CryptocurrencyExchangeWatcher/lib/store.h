/*
 * DO NOT MODIFY THE CONTENTS OF THIS FILE.
 * IT WILL BE REPLACED DURING GRADING
 */
#ifndef STORE_H
#define STORE_H

/*
 * Interface specifications for a simple key/value store.
 */

/*
 * Types of values that can be saved in the store.
 */
enum store_value_type {
    STORE_NO_TYPE,
    STORE_INT_TYPE,
    STORE_LONG_TYPE,
    STORE_DOUBLE_TYPE,
    STORE_STRING_TYPE
};

/*
 * Structure that represents a value that can be saved in the store.
 */
struct store_value {
    enum store_value_type type;
    union {
	int int_value;
	long long_value;
	double double_value;
	char *string_value;
    } content;
};

/**
 * @brief  Get the value associated with a specified key in the store.
 * The caller must use store_free_value() to free any value returned,
 * once it is no longer required.
 *
 * @param key  The key.
 * @return  The associated value, if any, otherwise NULL.
 */
struct store_value *store_get(char *key);

/**
 * @brief  Associate a value with a specified key in the store.
 * Any value previously associated with the key is removed.
 *
 * @param key  The key.
 * @param vp  The value.  If NULL, then after the call the key
 * will have no associated value in the store.
 * @return  0 if successful, -1 if error.
 */
int store_put(char *key, struct store_value *vp);

/**
 * @brief  Free a value returned by store_get().
 *
 * @param vp  The value to be freed.
 */
void store_free_value(struct store_value *vp);

#endif
