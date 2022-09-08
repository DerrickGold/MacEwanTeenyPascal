/*=============================================================================
 * Header inclusion
 *===========================================================================*/
%x endlessstr comments

%top{
/* CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * By: Derrick Gold
 */

#include <stdio.h>
#include <string.h>

#ifndef YYSTYPE

typedef union {
  int value;
  char *string;
}yystype;

#define YYSTYPE yystype

#endif /* YYSTYPE */

#ifdef yyIN_HEADER

extern YYSTYPE yylval;

#else /*not def yyIN_HEADER*/
#include "parser.h"
#include "tokens.h"
YYSTYPE yylval;

#endif /*not def yyIN_HEADER*/

//Silence flex from repeating input tokens to stdout
#define ECHO do {} while (0)


#define SET_YYLVAL_STR(type) do { \
  yylval.string=yytext; return (type); \
} while (0)

#define SET_YYLVAL_ERR(msg) do { \
  yylval.string=(char*)msg; return TOK_ERROR; \
} while (0)

/*
 * Teeny pascal utilizes #XX for hex values, which are also recognized as
 * numeric constants, thus, are just converted to decimal internally
 */
#define SET_YYLVAL_NUM(type) do { \
if (yytext[0] == '#') yylval.value = strtol(&yytext[1], NULL, 16); \
   else yylval.value = strtol(yytext, NULL, 10); \
   return (type); \
} while (0)

//Test if token is eof or system error
#define LEXTOKEN_ISEOF(token) ((token) == TOK_ENDFILE || (token) == TOK_SYSERR)


//defines for getToken state machine
typedef enum {
  GETTOKEN_STATE_EXIT = -1,
  GETTOKEN_STATE_DEFAULT,
  GETTOKEN_STATE_ENDFILE
  
} GETTOKEN_STATES;


//Token struct returned by getToken
typedef struct LexToken_s {
  int line;
  int type;
  yystype lexeme;
} LexToken_t;


extern LexToken_t Lexer_getToken();

extern void Lexer_printToken(LexToken_t token, FILE *output);

extern void Lexer_lexemeAsString(LexToken_t token, char *outBuf, size_t outBufSize);

extern void Lexer_freeToken(LexToken_t *token);

extern LexToken_t *Lexer_heapifyToken(LexToken_t token);

extern void Lexer_tokenDestructor(LexToken_t *token);
}
  /*==========================================================================
  * end of header inclusion (top%)
  **=========================================================================*/

  /* Some basic definitions */
letter                      [a-zA-Z]
digit                       [0-9]
digits                      {digit}+
decimal_literal             {digits}

hex_letter                  [a-fA-F]
hex_digit                   {digit}|{hex_letter}
hex_number                  {hex_digit}{hex_digit}*
hex_literal                 \#{hex_number}

str_char                    {letter}|{digit}|[ \_:\?\.\,]

id                          {letter}({letter}|{digit}|\_)*
num                         {decimal_literal}|{hex_literal}
str                         \'{str_char}*\'
badStr                      \'({str_char}[^'])*

whitespace                  [ \t\n]
space                       {whitespace}
commentStart                \(\*
commentEnd                  \*\)
comma                       ,

noteq                       \<\>
lteq                        \<=
gteq                        \>=
assign                      :=



%%
  int commentDepth = 0;
  const char *STRING_ERR = "Unterminated String";

<INITIAL>{
  /* Keyword rules */
do                          { SET_YYLVAL_STR(TOK_KEY_DO); }
of                          { SET_YYLVAL_STR(TOK_KEY_OF); }
or                          { SET_YYLVAL_STR(TOK_KEY_OR); }
if                          { SET_YYLVAL_STR(TOK_KEY_IF); }
and                         { SET_YYLVAL_STR(TOK_KEY_AND); }
not                         { SET_YYLVAL_STR(TOK_KEY_NOT); }
var                         { SET_YYLVAL_STR(TOK_KEY_VAR); }
end                         { SET_YYLVAL_STR(TOK_KEY_END); }
shl                         { SET_YYLVAL_STR(TOK_KEY_SHL); }
shr                         { SET_YYLVAL_STR(TOK_KEY_SHR); }
div                         { SET_YYLVAL_STR(TOK_KEY_DIV); }
mod                         { SET_YYLVAL_STR(TOK_KEY_MOD); }
then                        { SET_YYLVAL_STR(TOK_KEY_THEN); }
case                        { SET_YYLVAL_STR(TOK_KEY_CASE); }
read                        { SET_YYLVAL_STR(TOK_KEY_READ); }
else                        { SET_YYLVAL_STR(TOK_KEY_ELSE); }
array                       { SET_YYLVAL_STR(TOK_KEY_ARRAY); }
begin                       { SET_YYLVAL_STR(TOK_KEY_BEGIN); }
while                       { SET_YYLVAL_STR(TOK_KEY_WHILE); }
write                       { SET_YYLVAL_STR(TOK_KEY_WRITE); }
const                       { SET_YYLVAL_STR(TOK_KEY_CONST); }
integer                     { SET_YYLVAL_STR(TOK_KEY_INTEGER); }


  /* Teeny Pascal Supported Symbols */
{comma}                     { SET_YYLVAL_STR(TOK_COMMA); }
\(                          { SET_YYLVAL_STR(TOK_LPAREN); }
\)                          { SET_YYLVAL_STR(TOK_RPAREN); }
;                           { SET_YYLVAL_STR(TOK_SEMICOLON); }
:                           { SET_YYLVAL_STR(TOK_COLON); }
=                           { SET_YYLVAL_STR(TOK_EQ); }
\.                          { SET_YYLVAL_STR(TOK_PERIOD); }
\>                          { SET_YYLVAL_STR(TOK_GREATER); }
\<                          { SET_YYLVAL_STR(TOK_LESS); }
\+                          { SET_YYLVAL_STR(TOK_PLUS); }
\-                          { SET_YYLVAL_STR(TOK_MINUS); }
\*                          { SET_YYLVAL_STR(TOK_STAR); }
{noteq}                     { SET_YYLVAL_STR(TOK_NOTEQ); }
{lteq}                      { SET_YYLVAL_STR(TOK_LTEQ); }
{gteq}                      { SET_YYLVAL_STR(TOK_GTEQ); }
{assign}                    { SET_YYLVAL_STR(TOK_ASSIGN); }
{commentStart}              { yymore(); commentDepth++; BEGIN(comments); }
{whitespace}                { }



  /* More complex lexemes */
{id}                        { SET_YYLVAL_STR(TOK_ID); }
{num}                       { SET_YYLVAL_NUM(TOK_NUM); }
{str}                       { SET_YYLVAL_STR(TOK_STR); }

  /* Errors
   *
   * The justification:
   *
   * So, I've decided to enforce the white space requirement
   * for keywords, identifiers, numbers, and strings under the
   * notion that later on, for error 'recovery' in the later
   * stages will be less complex.
   *
   * For example, an input of "12identifier" will be reported as a
   * token error with string "12identifier" rather than two separate
   * tokens. With two separate tokens, depending on the method of
   * error recovery, one would have to manage two tokens in the
   * event of the error, one which is clearly identified as an error
   * token, with the other a potential viable token. Thus, an extra
   * step would need to be taken to determine if the second token is
   * still valid. With the whitespace enforced, one can simply drop
   * the invalid token with no second guessing.
   *
   * */
(^{whitespace}|.)           { SET_YYLVAL_STR(TOK_ERROR); }
{num}{id}                   { SET_YYLVAL_STR(TOK_ERROR); }
{num}{str}                  { SET_YYLVAL_STR(TOK_ERROR); }
{str}{id}                   { SET_YYLVAL_STR(TOK_ERROR); }
{str}{num}                  { SET_YYLVAL_STR(TOK_ERROR); }
{id}{str}                   { SET_YYLVAL_STR(TOK_ERROR); }

<<EOF>>                     { SET_YYLVAL_STR(TOK_ENDFILE);}


{badStr} { yymore(); BEGIN(endlessstr); }

} /* end of INITIAL */

  /*multi-line strings are not supported, skip till end of file*/
<endlessstr>{

  .                         { }
  
  <<EOF>>                   { SET_YYLVAL_ERR(STRING_ERR); }
  
}

 /* Comment state. While in comment states, consume everything*/
<comments>{

  {commentStart}            {
    //for each comment start find, increase the comment depth
    commentDepth++;
  }
  
  {commentEnd}              {
    //if we hit the last commentEnd, we are no longer in comment
    if (--commentDepth == 0)
      BEGIN(INITIAL);
  }
  
  [^{commendEnd}]            { 
    //eat non-end comment tokens
  }
  
  <<EOF>>                   { SET_YYLVAL_STR(TOK_ENDFILE); }
}
 
%%

//Flex input continuation, return 1 to indicate we only
//need one file worth of scanning work to be done.
int yywrap() {
  return 1;
}

//Free any memory used by a token
void Lexer_freeToken(LexToken_t *token) {

  if (!token) return;

  if (token->type != TOK_NUM && token->type != TOK_ENDFILE  && token->lexeme.string)
    free(token->lexeme.string);
  token->lexeme.string = NULL;
}


LexToken_t Lexer_makeToken(int type, yystype lexeme, int lineNum) {

  LexToken_t newToken = {.line=lineNum, .type = type};
  newToken.lexeme.string = NULL;

  if (type == TOK_NUM) {
    //If the token is just a number, all we need to do is copy it over
    newToken.lexeme = lexeme;
  } else if (type != TOK_ENDFILE) {
    //Otherwise, we need to allocate space for the string value.
    size_t stringBufLen = strlen(lexeme.string) + 1;
    newToken.lexeme.string = calloc(stringBufLen, sizeof(char));

    //check the allocation...
    if (!newToken.lexeme.string) {
      fprintf(stderr, "_makeToken[%d]: Error allocating token string.", lineNum);
      newToken.type = TOK_SYSERR;
      return newToken;
    }
    strncpy(newToken.lexeme.string, lexeme.string, stringBufLen);
  }

  return newToken;
}

LexToken_t *Lexer_heapifyToken(LexToken_t token) {
  LexToken_t *t = calloc(1, sizeof(LexToken_t));

  if (!t) {
    fprintf(stderr, "Lexer_heapify: failed to allocate space for token.\n");
    return NULL;
  }

  //copy details over to new token
  //memcpy(t, &token, sizeof(LexToken_t));
  t->line = token.line;
  t->type = token.type;
  t->lexeme = token.lexeme;
  
  return t;
}

/*
 * Clean up for token that has been heapified.
 *
 */
void Lexer_tokenDestructor(LexToken_t *token) {
  Lexer_freeToken(token);
  if (token)
    free(token);
}

/*
 * Print out a token
 */
void Lexer_printToken(LexToken_t token, FILE *output) {

  const char *string = LEXER_TOKEN_STRINGS[token.type];
  fprintf(output, "%d, %s,", token.line, string);
  
  if (token.type == TOK_NUM)
    fprintf(output, " %d", token.lexeme.value);
  else if (token.lexeme.string != NULL)
    fprintf(output, " %s", token.lexeme.string);

}

void Lexer_lexemeAsString(LexToken_t token, char *outBuf, size_t outBufSize) {

  //if endfile token, clear the outbuffer
  if (token.type == TOK_ENDFILE) {
    outBuf[0] = '\0';
    return;
  }

  if (token.type == TOK_NUM) {
    //hopefully the max number of characters ever needed to hold an integer
    char buf[64]; 
    //snprintf does not exist in the C90 standard which this file is compiled with
    sprintf(buf, "%d", token.lexeme.value);
    strncpy(outBuf, buf, outBufSize);
  } else if (token.lexeme.string != NULL)
    strncpy(outBuf, token.lexeme.string, outBufSize);
  
}

/*
 * Returns the next token from the lexer.
 */
LexToken_t Lexer_getToken(void) {

  GETTOKEN_STATES state = GETTOKEN_STATE_DEFAULT;
  LexToken_t token = {};

  int tok = 0;
  while (state != GETTOKEN_STATE_EXIT) {

    switch (state) {
      default:
      //fall through incase state isn't set
      
      case GETTOKEN_STATE_DEFAULT:
        tok = yylex();
        //if end of file, or system error, exit
        if (LEXTOKEN_ISEOF(tok)) {
           state = GETTOKEN_STATE_ENDFILE;
           
        } else {
          //otherwise, grab the token and exit
          token = Lexer_makeToken(tok, yylval, yylineno);
          state = GETTOKEN_STATE_EXIT;
        } 

        break;

        //fall through if end file hit
      case GETTOKEN_STATE_ENDFILE:
      
        //return the token and exit the state machine
        token = (LexToken_t) {
          .line = yylineno,
          .type = tok,
          .lexeme.string = NULL
        };
        state = GETTOKEN_STATE_EXIT;

        break;
        
    } //end of switch
    
  } //end of while

  return token;
}




