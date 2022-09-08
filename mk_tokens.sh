#!/usr/bin/env bash

PRGM_NAME="$(basename $0)"


# Files to operate on/with
CFILE="tokens.c"
HFILE="tokens.h"
INPUT="parser.h"

# Some string constants
HEADERGUARD="LEXER_TOKENS__H___"
TOKEN_DECL="const char *LEXER_TOKEN_STRINGS[]"

TOKEN_DEF="#define "

#extra tokens to define and create strings for
EOF_TOKEN="TOK_ENDFILE"
EXTRA_TOKENS=(
    "$EOF_TOKEN" \
     "TOK_COMMENT_START" "TOK_COMMENT_END" \
     "TOK_NEWLINE" \
)

#
# Define any specific token values.
# EOF should be 0 for lemon's sake.
#
declare -A TOKEN_MAP=(["$EOF_TOKEN"]="0")

#
# Generates the header file to access the token string array
# globally within the project.
#
makeHeader() {
    #delete old header file
    if [ -f "$HFILE" ]; then
	rm "$HFILE"
    fi

    #the array declaration
    echo "#ifndef $HEADERGUARD
#define $HEADERGUARD
extern $TOKEN_DECL;
#endif" >> "$HFILE"

}

#
# Writes out a token for use as a string in the
# C file this script generates
#
printToken() {
    printf "\t\"%s\",\n" "$1"
}


#
# Generates a C file containing an array of strings
# for each token to be printed.
#
makeCFile() {
    
    #delete old C file 
    if [ -f "$CFILE" ]; then
	rm "$CFILE"
    fi

    #declare the array
    echo "$TOKEN_DECL = {" > "$CFILE"
    

    tokenCount=0

    #pad position 0 in the string array, token numbers start at 1
    printToken "$EOF_TOKEN" >> "$CFILE"
    
    while read line; do

	#skip lines without #define leading them
	if [[ $line != "#define"* ]]; then
	    continue
	fi
	
	# Get the token name from the input file
	# which follows the format of
	#
	# #define TOKEN_NAME		NUM
	#
	# The tokens are defined in the order of their
	# numerical value.
	tok=$(echo "$line" | awk '{print $2}')
	if [ ! "$tok" = "$EOF_TOKEN" ]; then
		printToken $tok >> "$CFILE"
		((tokenCount++))
	fi
	
    done < "$INPUT"

    #end the array
    printf "\t\"\"\n};" >> "$CFILE"
}


#
# Adds new token defines to the input file
# that token strings are being generated
# from.
#
# These new tokens are extra ones defined
# typically for error detection.
#
defineToken() {
    offset=$3
    printf "%-*s" $offset "$TOKEN_DEF$1" >> "$INPUT"
    printf "$2\n" >> "$INPUT"
}

#
# Adds a message to indicate start of $INPUT file modification
# by this script
# 
modMessage() {
    printf "\n/*======Modified By $PRGM_NAME======*/\n" >> "$INPUT"
}


#
# Adds new tokens defined by $EXTRA_TOKENS to the
# input file (parser.h)
#
addNewTokens() {
    #get length of token line to determine which column to place token number in
    line=$(head -n 1 "$INPUT")
    #the token number is in the last column of the line
    column=${#line}
    #subtract one since that is where the number will be written to
    ((column--))

    #get the last token number
    tokNum=$(awk '{print $3}' "$INPUT" | tail -n 1)
    ((tokNum++))

    #set indication that we are modifying parser.h file to add our own tokens
    modMessage
    
    for token in "${EXTRA_TOKENS[@]}"; do
	#add token define to parser.h
	
	#check if the token has a specific value to apply
	if [ ${TOKEN_MAP[$token]+isset} ]; then
	    #set the token to a specific value
	    num=${TOKEN_MAP[$token]}
	    defineToken $token $num $column
	else
	    #normal behavior, keep counting
	    defineToken $token $tokNum $column
	    ((tokNum++))
	fi
    done
}


if [ ! -f "$INPUT" ]; then
    echo "Missing token list file: $INPUT"
    exit 1
fi

addNewTokens
makeHeader
makeCFile


#exit success
exit 0

