#!/bin/bash

# CMPT 399 (Winter 2016)
# Assignment 2: Syntactic Analysis
# By Derrick Gold

# Compares the output of $inputScript when run through $program
# to the expected output in $compareScript.

# Returns success when the output matches the expected output.



# CURDIR obtains the directory in which this script exists in at run time,
# to use as a reference point for relative file paths later on.

# For example, the location of "mtp" is "../../mtp" from the location
# of where this script resides--indepedent from where it is executed in the shell.
THISNAME="$( basename $0)"
CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
program="$CURDIR/../../mtp"
pFlags="-v"
inputScript="$1"

compareScript="$2"

printHelp() {
    echo "$THISNAME <input script> <expected file>
	Tests if the input script output matches the expected output.

	input script:	script to run through the compiler
	expected file:	file with expected output for the input script
"
}

#
# Check if the program binary exists
#
if [ ! -x $program ]; then
    echo "[Error]: $program not found."
    exit 1
fi



# Check that both input and compare file
if [ -z "$inputScript" ] || [ -z "$compareScript" ]; then
    printHelp
    exit 0
fi


# run the program first
output=$($program $pFlags "$inputScript" 2>&1)

if [ -z "$output" ]; then
    echo "[Error]: $inputScript did not produce any output"
    exit 1
fi

# then run diff on it
result=$(diff "$compareScript" <(echo "$output"))

if [ -z "$result" ]; then
    echo "[Success]: $(basename $1) completed."
else
    echo "[Failed]: $(basename $1) unexpected output."
    echo "$result"
    exit 1
fi

exit 0


