/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Lemon parser API
 *
 */
#ifndef __PARSERHELPER__H_
#define __PARSERHELPER__H_

#include "lexer.h"

/*
 * Various statuses the parser can return, indicating
 * success, or one of many potential errors.
 */
typedef enum {
  PARSE_INPROGRESS,
  PARSE_SUCCESS,
  PARSE_SYNTAXERR,
  PARSE_STACKOFERR,
  PARSE_FAILED,
  NODE_ALLOC_ERR,
  NODE_SIB_ERR,
  NODE_CHILD_ERR,
} ParseStatus;


/*
 * ParseAlloc:
 *      Creates a parser instance.
 *
 * Arguments:
 *      mallocProc: Function to allocate memory with.
 *
 * Returns:
 *      Pointer to allocated parser instance.
 */
extern void *ParseAlloc(void *(*mallocProc)(size_t));


/*
 * ParseFree:
 *      Frees memory used by a parser instance.
 *
 * Arguments:
 *      p: Parser instance pointer.
 *      fn: Function to free memory with.
 *
 */
extern void ParseFree(void *p, void (*fn)(void*));

/*
 * Parse:
 *      Parses input tokens in accordance to a syntax
 *      defined in the 'parser.y' file.
 *
 * Arguments:
 *      yyp: Parser instance pointer.
 *      yymajor: Token type as defined in the 'parser.h' file.
 *      data: Associated data with token type. In this program's case,
 *              a pointer to a LexToken_t structure.
 */
extern void Parse(void *yyp, int yymajor, LexToken_t *data);

/*
 * Parser_getTree:
 *      Get the abstract syntax tree created by the parser.
 *
 * Returns:
 *      A TreeNode_t pointer to the root of the abstract syntax
 *      tree.
 */
extern TreeNode_t *Parser_getTree(void);

/*
 * Parser_getStatus:
 *      Get the current state of the parser.
 *
 * Returns:
 *      Returns a status defined by the ParseStatus enum.
 */
extern ParseStatus Parser_getStatus(void);

/*
 * Parser_hasError:
 *      Determines if the parser has encountered an error.
 *
 * Returns:
 *      True if an error occurred, false otherwise.
 */
extern bool Parser_hasError(void);

/*
 * Parser_getErrorStr:
 *      Get an error message based on the parser's status.
 *
 * Returns:
 *      Pointer to error message string array.
 */
extern const char *Parser_getErrorStr(void);

#endif //__PARSERHELPER__H_
