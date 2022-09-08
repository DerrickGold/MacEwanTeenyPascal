/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * C Library for linking with produced assembly by
 * this compiler. 
 */
#ifndef __IO_LIB__H
#define __IO_LIB__H

#include <stdarg.h>

/*
 * read:
 *  Read in integers
 *
 * Arguments:
 *  count: number of integers to read into
 *  ...: integer addresses to store values read in. One address
 *        per 'count' of integers.
 */
void read(int count, ...);

/*
 * write:
 *  Write out data.
 *
 * Arguments:
 *  count: the number of elements to write out
 *  ...: type and value pair of data to write out. Each value
 *    Each element to write out must include a type:
 *      INTEGER, STRING, or BOOLEAN
 *
 *    and a storage location for that element.
 *
 *  e.g. writing out a Boolean value and a string:
 *    write(2, BOOLEAN, (1 > 0), STRING, "Hello, World");
 */
void write(int count, ...);

#endif
