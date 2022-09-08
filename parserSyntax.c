/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Lemon parser syntax error reporting API.
 *
 */
#include <stdio.h>
#include <string.h>

#include "parserSyntax.h"
#include "tokens.h"   //get token text
#include "parser.h"     //get token definitions
#include "tree.h"     //get node types/node type text
#include "defines.h"


/* The following are used for listing expected tokens. */
#define DELIM " or"
#define QUOTE "'"
#define PREFIX "\n  "
#define NTPREFIX PREFIX"Non-terminal: "

/* Some shorthand defines for later on. */
#define SUGGEST_TERMINAL(terminal) do {                               \
    msgtype = SYNTAX_MSG_POTENTIAL;                                   \
    char err = SyntaxErr_addSuggestion(tokList, listSize,             \
                                     LEXER_TOKEN_STRINGS[(terminal)], \
                                     SUGGESTION_TERMINAL);            \
    if (err)                                                          \
      return SYNTAX_MSG_ERR;                                          \
  } while (0)


#define SUGGEST_NONTERM(nonterm) do {                             \
    msgtype = SYNTAX_MSG_POTENTIAL;                               \
    char err = SyntaxErr_addSuggestion(tokList, listSize,         \
                                       NODE_TYPE_TEXT[(nonterm)], \
                                       SUGGESTION_NONTERMINAL);   \
    if (err)                                                      \
      return SYNTAX_MSG_ERR;                                      \
  } while (0)


/* Cause header marks the start of Non-terminal suggestions
 * in the syntax error msg.
 */
#define CAUSE_HEADER(reason) do {                                       \
    char err = SyntaxErr_addSuggestion(tokList, listSize,               \
                                       SYNTAX_SUGGESTIONS[(reason)],    \
                                       SUGGESTION_REASONING);           \
    if (err)                                                            \
      return SYNTAX_MSG_ERR;                                            \
  } while (0)


const char *SYNTAX_SUGGESTIONS[] = {
  "Error generating syntax suggestions.",
  "expected a token type of: ",
  "likely missing a(n) ",
  "\nPossibly due to malformed or missing:"
};


static char *SYNTAX_MSG = "Syntax Error: line %d:\n"
  "Unexpected token (%s, '%s'), %s %s\n";



/*
 * Decorates an input token string with a prefix and delimiter
 * for fancy printing.
 */
static char *formatExpectedToken(const char *nextToken, char isNonTerminal) {

  const char *delim = DELIM;
  const char *quote = QUOTE;
  const char *prefix = PREFIX;

  //indicate token is nonterminal in the token decoration
  if (isNonTerminal)
    prefix = NTPREFIX;

  //allocate string with space for all decorations
  size_t expectedLen = strlen(prefix) + strlen(nextToken) + (strlen(quote) * 2) +
                       strlen(delim) + 1;

  char *buf = calloc(expectedLen + 1, sizeof(char));
  if (!buf) {
    fprintf(stderr, "Error allocating expected token space.\n");
    return NULL;
  }

  //copy the string over to the new allocation, with its decorators.
  SAFECAT(buf, prefix, expectedLen);
  SAFECAT(buf, quote, expectedLen);
  SAFECAT(buf, nextToken, expectedLen);
  SAFECAT(buf, quote, expectedLen);
  SAFECAT(buf, delim, expectedLen);

  return buf;
}

/*
 * Removes the last delimiter in the token list generated through 
 * SyntaxErr_addSuggestion. Because it is unknown ahead of time how
 * many tokens or suggestions are to be printed out, it is easier
 * to just print out everything first, then delete what is not
 * needed.
 */
static void removeLastDelimiter(char *outbuf, char *delim) {

  if (!outbuf)
    return;

  char *ptr = strstr(outbuf, delim);
  if (!ptr)
    return;

  char *next = NULL;

  //get the last delimiter of the string
  while ((next = strstr(ptr + strlen(delim), delim))) {
    ptr = next;
  }

  //lop off the delimiter
  if (ptr)
    *ptr = '\0';
}


char SyntaxErr_addSuggestion(char **tokList, size_t *listSize, const char *nextToken,
           char suggestionType) {

  if (nextToken == NULL)
    return -1;

  // Check if token  already exists, and skip if it does.
  if (*tokList) {
    char *exists = strstr(*tokList, nextToken);
    if (exists)
      return 0;
  }

  char tokenAllocd = 0;
  char *formattedToken = (char *) nextToken;

  //reasons added to the tokList are not decorated, just copy them over.
  if (suggestionType != SUGGESTION_REASONING) {
    formattedToken = formatExpectedToken(nextToken, suggestionType);
    if (!formattedToken)
      return -1;
    tokenAllocd = 1;
  }

  size_t expectedLen = strlen(formattedToken) + 1;

  //do we need to embiggen the tokList buffer?
  if (!*tokList || strlen(*tokList) + expectedLen > *listSize) {
    *listSize += expectedLen;

    char *tempBuf = realloc(*tokList, *listSize * sizeof(char));
    if (!tempBuf) {
      fprintf(stderr, "Failed to allocate list of expected tokens");

      //ditch the decorated token if it doesn't fit
      if (tokenAllocd)
        free(formattedToken);
      return -1;
    } else if (*tokList == NULL) {
      //if this is the first allocation, set the first element to '\0'
      //for strncat to start at the beginning of the allocation.
      tempBuf[0] = '\0';
    }

    *tokList = tempBuf;
  }

  //add token to the list
  //strncat(*tokList, formattedToken, strlen(formattedToken));
  SAFECAT(*tokList, formattedToken, *listSize);

  //no longer need the decorated token after this
  if (tokenAllocd)
    free(formattedToken);

  return 0;
}

/*
 * Some expected tokens can be easily predicted
 * if the unexpected token happens to be one used
 * in a very specific situation.
 *
 * For example, TOK_PERIOD marks the end of the program.
 * If TOK_PERIOD is the unexpected token, we can also suggest
 * that TOK_KEY_END might be necessary.
 *
 * In some cases, the tokens predicted may not make sense. They 'technically'
 * would resolve the issue, but may create another. In this situtation, it is 
 * possible that a non-terminal is being expected.
 */
SyntaxMsgType SyntaxErr_extraSuggestions(char **tokList, size_t *listSize, int tokenType) {

  SyntaxMsgType msgtype = SYNTAX_MSG_DEFINITE;

  switch (tokenType) {
  default:
    //no suggestions for given token type, and there exists some prior suggestions
    break;
    
  case TOK_PERIOD:
    SUGGEST_TERMINAL(TOK_KEY_END);
    break;

  case TOK_COLON:
    /* if no suggestions made, chances are, an id is missing */
    if (!*listSize)
      SUGGEST_TERMINAL(TOK_ID);
    break;

  case TOK_SEMICOLON:
    /* if semicolon is unexpected, and no previous suggestions exist, it's likely a missing
     * right parenthesis.
     */
    if (!*listSize)
      SUGGEST_TERMINAL(TOK_RPAREN);
    /*
     * before a semi colon typically expecting a variable declaration, const declaration,
     * or statement
     */
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(VARIABLE);
    SUGGEST_NONTERM(TYPE);
    break;
    
  case TOK_RPAREN:
    SUGGEST_TERMINAL(TOK_LPAREN);
    
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(CONSTANT);
    SUGGEST_NONTERM(EXP);
    SUGGEST_NONTERM(VARIABLE);
    break;
    
    /* For all multiply operators, a factor non-terminal may be suggested */
  case TOK_KEY_DIV:
  case TOK_KEY_MOD:
  case TOK_KEY_AND:
  case TOK_KEY_SHR:
  case TOK_KEY_SHL:
  case TOK_STAR:
    
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(FACTOR);
    break;
    
    /* Assignment definitely needs some id to assign to */
  case TOK_ASSIGN:
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(VARIABLE);
    break;
    
    /*Then token should suggest a condition non-terminal or right parenthesis*/
  case TOK_KEY_THEN:
    SUGGEST_TERMINAL(TOK_RPAREN);
    
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(CONDITION);
    break;
    
    /* If identifiers, strings, or numbers are causing issues, they are probably part of a condition
     * for an if or while statement.
     */
  case TOK_KEY_VAR:
  case TOK_KEY_CONST:
  case TOK_KEY_BEGIN:
  case TOK_KEY_WRITE:
  case TOK_KEY_READ:
  case TOK_ID:
  case TOK_STR:
  case TOK_NUM:
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(IF_STMT);
    SUGGEST_NONTERM(WHILE_STMT);
    SUGGEST_NONTERM(WRITE_STMT);
    SUGGEST_NONTERM(READ_STMT);
    SUGGEST_NONTERM(CASE);
    break;
    
    /* Prior to an else must be an if statement or case statement*/
  case TOK_KEY_ELSE:
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(IF_STMT);
    SUGGEST_NONTERM(CASE_STMT);
    break;
    
    /* if Do is unexpected, its likely that a while statement or condition is malformed*/
  case TOK_KEY_DO:
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(WHILE_STMT);
    SUGGEST_NONTERM(CONDITION);
    break;

    /* Used only in case statements and array types, if this is unexpected, probably due
     * to a malformed expression or array
     */
  case TOK_KEY_OF:
    CAUSE_HEADER(SYNTAX_MSG_REASONING_1);
    SUGGEST_NONTERM(ARRAY);
    SUGGEST_NONTERM(EXP);
    break;
  }
  
  //finally, print out the syntax error message  
  if (!*listSize) {
    //the the expectedStringBuf didn't change, i.e. no suggestions added
    //just guess that a semicolon might be missing.
    SUGGEST_TERMINAL(TOK_SEMICOLON);
  }

  removeLastDelimiter(*tokList, DELIM);
  return msgtype;
}

void SyntaxErr_printMsg(FILE *output, LexToken_t *unexpected, char *tokList, SyntaxMsgType msgType) {

  if (!unexpected) {
    fprintf(output, "No token available\n");
    return;
  }
  //get line number
  int lineno = unexpected->line;

  //get the type string for the unexpected token
  const char *unexpectedType = LEXER_TOKEN_STRINGS[unexpected->type];

  //get the actual text the token contains for reporting
  char unexpectedText[256];
  Lexer_lexemeAsString(*unexpected, unexpectedText, sizeof(unexpectedText));

  //print out the syntax error message.
  fprintf(output, SYNTAX_MSG, lineno, unexpectedType, unexpectedText, SYNTAX_SUGGESTIONS[msgType], tokList);
}


