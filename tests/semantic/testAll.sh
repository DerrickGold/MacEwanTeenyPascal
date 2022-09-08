#!/bin/bash

# CMPT 399 (Winter 2016)
# Assignment 3: Semantic Analysis
# By Derrick Gold

# A wrapper for "runTests.sh" to run all the tests within the directory this
# script resides in.


# CURDIR obtains the directory in which this script exists in at run time,
# to use as a reference point for relative file paths later on.

# For example, the location of "runTests.sh" is "../bin/runTests.sh" from the
# location of where this script resides--indepedent from where it is executed in
# the shell.
CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


#change to ../bin directory and run ./runTests.sh on this folder "lexer"
cd "$CURDIR/../bin" && ./runTests.sh "$CURDIR"
