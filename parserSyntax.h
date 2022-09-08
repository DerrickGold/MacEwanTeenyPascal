/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Lemon parser syntax error reporting API.
 *
 */
#ifndef __PARSER_SYNTAX__H_
#define __PARSER_SYNTAX__H_

#include "lexer.h"

/*
 * These suggestion defines determine how a "token" is
 * decorated in the SyntaxErr_addSuggestion function.
 * Non-terminals have a prefix indicating that those suggestions
 * are not token suggestions. While reasonings, are simply strings
 * that provide a context as to why a particular suggestion
 * has been made.
 */
#define SUGGESTION_TERMINAL     0
#define SUGGESTION_NONTERMINAL  1
#define SUGGESTION_REASONING    2

/*
 * This enum maps to a string in the SYNTAX_SUGGESTION
 * array (in parserSyntax.c) and indicates whether a suggestion
 * has some human insight or not (see SyntaxErr_extraSuggestions).
 */
typedef enum {
  SYNTAX_MSG_ERR,
  SYNTAX_MSG_DEFINITE,
  SYNTAX_MSG_POTENTIAL,
  SYNTAX_MSG_REASONING_1,
} SyntaxMsgType;

/*
 * SyntaxErr_addSuggestion:
 *  Add a token or string to a buffered list for printing
 *  out later.
 *
 * Arguments:
 *  tokList: The pointer to the string buffer in which the suggestions
 *    are being stored.
 *  listSize: Pointer to an integer holding the current size of the tokList.
 *  nextToken: The next token or string to add to the suggestion list.
 *  suggestionType: SUGGESTION_TERMINAL, SUGGESTION_NONTERMINAL, or 
 *      SUGGESTION_REASONING.
 *
 * Returns:
 *  0 if the function succeeded. -1 for failure. 
 */
extern char SyntaxErr_addSuggestion(char **tokList, size_t *listSize,
                                    const char *nextToken, char suggestionType);
/*
 * SyntaxErr_extraSuggestions:
 *  Attempts to add extra suggestions for syntax errors based
 *  on human knowledge of known token order and precedence.
 *  
 *
 * Arguments:
 *  tokList: The pointer to the string buffer in which the suggestions
 *    are being stored.
 *  listSize: Pointer to an integer holding the current size of the tokList.
 *  tokenType: The type of the unexpected token as defined in "parser.h"
 *
 * Returns:
 *  A syntax error message type as defined by the SyntaxMsgType enum.
 *  If any extra suggestions were made, the syntax error message will
 *  reflect them in the message type.
 */
extern SyntaxMsgType SyntaxErr_extraSuggestions(char **tokList, size_t *listSize,
                                                int tokenType);
/*
 * SyntaxErr_printMsg:
 *  Prints a formatted syntax error message.
 *
 * Arguments:
 *  output: File stream to write syntax error message to.
 *  unexpected: The unexpected token that causes this message to appear.
 *  tokList: The pointer to the generated tokList through adding suggestions.
 *  msgType: The message type determined by SyntaxErr_extraSuggestions.
 */
extern void SyntaxErr_printMsg(FILE *output, LexToken_t *unexpected,
                               char *tokList, SyntaxMsgType msgType);

#endif //__PARSER_SYNTAX__H_
