/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Some useful defines
 */

#ifndef __DEFINES_H___
#define __DEFINES_H___

#define EMPTY_STR ""

//buffer size to use when converting integers to strings
#define NUM_TO_STR_BUF 32

//number of bytes a word is for the target architecture
#define WORD_SIZE_BYTES 4
#define WORD_SIZE_BYTES_STR "4"

/*
 * Ensures we aren't concatenating past any buffer limits
 */
#define SAFECAT(dest, src, destSize) do {     \
    size_t remaining = destSize - strlen(dest),       \
    srcSize = strlen(src);            \
  if (remaining < srcSize)            \
    strncat(dest, src, remaining);          \
  else                  \
    strncat(dest, src, srcSize);          \
} while (0)



#endif //end __DEFINES_H___
