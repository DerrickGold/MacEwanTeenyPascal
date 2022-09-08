#!/bin/bash


# CMPT 399 (Winter 2016)
# Assignment 2: Syntactic Analysis
# By Derrick Gold

# Runs "prgmTest.sh" in this directory for each pair of .mtp
# and .expected files within a directory specified by $TESTDIR.

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

#each item is now delimited by a newline instead of all whitespaces
IFS=$'\n'
for f in $(find "$TESTDIR" -depth 1 -type f |  egrep ".*\.$INPUT_EXT" | \
			sed -e "s/\.$INPUT_EXT//"); do
    IFS=$OLDIFS
    "$TESTSCRIPT" -v "$f"."$INPUT_EXT" &>  "$f"."$EXPECTED_EXT"
    IFS=$'\n'
done

#revert IFS back to what it was before
IFS=$OLDIFS




