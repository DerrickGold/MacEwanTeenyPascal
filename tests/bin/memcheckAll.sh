#!/bin/bash


# CMPT 399 (Winter 2016)
# Assignment 2: Syntactic Analysis
# By Derrick Gold

# Runs the MacEwan Tiny Pascal compiler in valgrind for
# every test program in $TESTDIR. A successful test is one
# in which no memory leaks can be detected (0 bytes in 0 blocks on the heap)

# Returns the number of tests passed out of the total number of tests ran.

# CURDIR obtains the directory in which this script exists in at run time,
# to use as a reference point for relative file paths later on.

# For example, the location of "prgmTest" is "./mtp" from the location
# of where this script resides--indepedent from where it is executed in the shell.
THISNAME="$( basename $0 )"
CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TESTSCRIPT="$CURDIR/../../mtp"

TESTDIR="$1"

#file extensions of programs to run through mtp
INPUT_EXT="mtp"

#file extensions to compare output of input files
EXPECTED_EXT="expected"

#backup IFS before changing it
OLDIFS=$IFS


totalTests=0
successful=0

#each item is now delimited by a newline instead of all whitespaces
IFS=$'\n'
for f in $(find "$TESTDIR" -depth -type f |  egrep ".*\.$INPUT_EXT" | \
			sed -e "s/\.$INPUT_EXT//"); do

    (( totalTests++ ))
    bytesLeft=$(valgrind "$TESTSCRIPT" "$f"."$INPUT_EXT" 2>&1| egrep -o "in use at exit: [0-9,]+" | egrep -o "[0-9,]+")
    if [ "$bytesLeft" = "0" ]; then
	(( successful++ ))
	printf "[Success]"
    else
	printf "[Fail]"
    fi
    printf " Testing: $f.$INPUT_EXT\n"   
done

#revert IFS back to what it was before
IFS=$OLDIFS

echo "$successful/$totalTests tests passed."


