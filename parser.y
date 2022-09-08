%include{
/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
s * Author: Derrick Gold
 *
 * This file contains code for constructing an abstract
 * syntax tree based on a defined grammar in accordance to
 * the lemon parser specification.
 */
#include <assert.h>
#include "lexer.h"
#include "tree.h"
#include "tokens.h"
#include "parserHelper.h"
#include "parser.h"
#include "parserSyntax.h"


/*
 * If any errors occur adding a child or sibling
 * to a node, print out the conflicting nodes.
 */
#define NODE_DBG_MSG(node1, node2) do { \
  fprintf(stderr, "Failed adding: \n"); \
  TreeNode_printNode(stderr, node2, false);  \
  fprintf(stderr, "To parent node: \n"); \
  TreeNode_printNode(stderr, node1, false);  \
} while (0)

#define PARSER_ERR_MSG fprintf(stderr, "%s\n", Parser_getErrorStr())

static const char *PARSER_STATUS_STRINGS[] = {
  "Parser still in progress.",
  "Successfully parsed token stream.",
  "Parser stopped: syntax error.",
  "Parser stopped: stack overflow.",
  "Parser stopped: failed to recover from error.",
  "Parser stopped: failed node allocation.",
  "Parser stopped: failed to add sibling node.",
  "Parser stopped: failed to add child node.",
};

//set status to in progress by default
static ParseStatus errors = PARSE_INPROGRESS;

static TreeNode_t *theTree = NULL;


/*
 * Public accessor for the abstract syntax tree built
 * by the parser.
 */
TreeNode_t *Parser_getTree(void) {
  return theTree;
} 

/*
 * Get the state of the parser. Allows one to check
 * if the parser encountered an error from outside
 * of the %syntax error directive.
 *
 */
ParseStatus Parser_getStatus(void) {
  return errors;
}

/*
 * Returns true if the parser encountered an error.
 */
bool Parser_hasError(void) {
  ParseStatus status = Parser_getStatus();
  return !(status == PARSE_SUCCESS || status == PARSE_INPROGRESS);
}

/*
 * Returns a message based on the current parser status.
 * Statuses include success as well as errors.
 */
const char *Parser_getErrorStr(void) {
  return PARSER_STATUS_STRINGS[Parser_getStatus()];
}



/*
 * Wrapper functions to catch tree library errors and
 * report them appropriately.
 */
static TreeNode_t *parser_mkNode(NodeType type, LexToken_t *token) {

  //exit if errors already exist
  if (Parser_hasError())
    return NULL;

  TreeNode_t *newNode = TreeNode_newNode(type, token);
  if (!newNode) {
    //set parser error
    errors = NODE_ALLOC_ERR;
    PARSER_ERR_MSG;
    return NULL;
  }

  return newNode;
}

static void parser_addSibling(TreeNode_t *node, TreeNode_t *newSib) {

  //exit if errors already exist
  if (Parser_hasError())
    return;

  if (!TreeNode_addSibling(node, newSib)) {
    errors = NODE_SIB_ERR;
    NODE_DBG_MSG(node, newSib);
    PARSER_ERR_MSG;
  }
  
}

static void parser_addChild(TreeNode_t *parent, TreeNode_t *child, int slot) {

  //exit if errors already exist
  if (Parser_hasError())
    return;

  //if (!TreeNode_addChild(parent, child)) {
  if (!TreeNode_setChild(parent, child, slot)) {
    errors = NODE_CHILD_ERR;
    NODE_DBG_MSG(parent, child);
    PARSER_ERR_MSG;
  }
  
}




 
} //end of %include

%start_symbol   program

%default_type     { TreeNode_t * }/* for non-terminals */
%token_type       { LexToken_t * }/* for a token's attributes */

%default_destructor       {
  TreeNode_destroy($$);
}

%token_destructor {
  Lexer_tokenDestructor($$);
}

/*
 * Create destructors for error tokens. %token_destructor
 * fails to destruct tokens that are not defined in any of
 * the productions below. By explicitly defining token
 * destructors here, lemon can catch error tokens passed  
 * into it by flex.
 */
%destructor TOK_ERROR {
  Lexer_tokenDestructor($$);
}

%destructor TOK_SYSERR {
  Lexer_tokenDestructor($$);
}

/*
 * Set right associativity on THEN and ELSE fixing the
 * ambiguity of an if then else statement where the else is
 * optional.
 */ 
%left TOK_KEY_IF.
%right TOK_KEY_THEN TOK_KEY_ELSE.




/*
 * Source for expected token finding:
 * http://stackoverflow.com/questions/11705737/expected-token-using-lemon-parser-generator
 *
 *
 */
%syntax_error {
  //set the parser status
  errors = PARSE_SYNTAXERR;

  //get the current token that lemon was working on
  LexToken_t *tok = TOKEN;
  
  //string for holding list of expected tokens
  char *suggestions = NULL;
  size_t suggestionsSize = 0;

  //loop through all the token names
  int i = 0, n = sizeof(yyTokenName) / sizeof(yyTokenName[0]);
  for (i = 0; i < n; ++i) {

    //and try one of the tokens and see if there exists a plausible action
    int a = yy_find_shift_action(yypParser, (YYCODETYPE)i);
    if (a >= YYNSTATE + YYNRULE) continue;

    //get the expected token string
    const char *expected = yyTokenName[i];

    if (SyntaxErr_addSuggestion(&suggestions, &suggestionsSize, expected,
      SUGGESTION_TERMINAL)) {
      
      if (suggestions && suggestionsSize > 0) {
        free(suggestions);
        suggestions = NULL;
        suggestionsSize = 0;
      }
    }
  }

  /* Get some extra suggestions that may not be apparent by lemon based on token type */
  SyntaxMsgType msg = SYNTAX_MSG_DEFINITE;
  if (tok)
    msg = SyntaxErr_extraSuggestions(&suggestions, &suggestionsSize, tok->type);
  

  if (msg == SYNTAX_MSG_ERR) {
    //if there was an error generating suggestions, clean up the useless message.
    if (suggestions)
      free(suggestions);
  }
  
  SyntaxErr_printMsg(stderr, tok, suggestions, msg);
 
  //do cleanup
  if (suggestions)
    free(suggestions);
  suggestions = NULL;
  suggestionsSize = 0;
  //lemon code will continue on from here
} //end of syntax error

//set parser status as stack overflow
%stack_overflow {
  errors = PARSE_STACKOFERR;
}

//set parser status as fully done
%parse_accept {
  //successfully parsed all tokens
  errors = PARSE_SUCCESS;
}

%parse_failure {
  errors = PARSE_FAILED;  
}

program ::= block(A) TOK_PERIOD. {
  theTree = A;
}



/*
 * Statement list can be either one or more statements.
 */
statement_list(A) ::= statement(B). {
  A = parser_mkNode(STMT_LIST, NULL);
  parser_addChild(A, B, 0);
}


/*
 * if there is more than one statement, they are to be
 * separated with a semicolon.
 */
statement_list(A) ::= statement_list(B) TOK_SEMICOLON statement(C). {
  A = B;                 
  parser_addSibling(A->child[0], C);
}

/*
 * A statement can be many things
 */
statement(A) ::= block(B). {
  A = B;
  TreeNode_addType(A, BLOCK_STMT);
}

statement(A) ::= . {
  A = parser_mkNode(NULL_STMT, NULL);
}

statement(A) ::= assign_statement(B). {
  A = B;
}


statement(A) ::= case_statement(B) TOK_KEY_END. {
  A = B;
}

statement(A) ::= if_statement(B). {
  A = B;
}

statement(A) ::= while_statement(B). {
  A = B;
}

statement(A) ::= write_statement(B). {
  A = B;
}

statement(A) ::= read_statement(B). {
  A = B;
}


/*
 * Blocks can have an optional const and variable section, but must
 * have a begin and end terminal with a list of statements
 */

//constant section, variable section
block(A) ::= const_section(B) var_section(C) TOK_KEY_BEGIN statement_list(D) TOK_KEY_END. {
  A = parser_mkNode(BLOCK, NULL);
  TreeNode_addType(A, BLOCK_STMT);
  
  if (B)
    parser_addChild(A, B, 0);
    
  if (C)
    parser_addChild(A, C, 1);
    
  parser_addChild(A, D, 2);
}

/* Assignment statements
 *
 * So, simple_name only refers to identifiers of non-array type.
 * variable refers to identifiers that are indexed. It is valid
 * to assign to both indexed identifiers, and at this point in time,
 * ambigiously typed identifiers.
 */
assign_statement(A) ::= simple_name(B) TOK_ASSIGN expression(C). {
  A = parser_mkNode(ASSIGN_STMT, NULL);
  parser_addChild(A, B, 0);
  parser_addChild(A, C, 1);
}

assign_statement(A) ::= variable(B) TOK_ASSIGN(D) expression(C). {
  A = parser_mkNode(ASSIGN_STMT, D);
  parser_addChild(A, B, 0);
  parser_addChild(A, C, 1);
}


/* Case Statements */
//just case no else
case_statement(A) ::= TOK_KEY_CASE expression(B) TOK_KEY_OF case_list(C). {
  A = parser_mkNode(CASE_STMT, NULL);
  parser_addChild(A, B, 0);
  parser_addChild(A, C, 1);
}

//case else
case_statement(A) ::= case_statement(B) TOK_KEY_ELSE statement(C). {
  A = B;
  parser_addChild(A, C, 2);
}


/* If statements */
if_statement(A) ::= TOK_KEY_IF condition(B) TOK_KEY_THEN statement(C). {
  A = parser_mkNode(IF_STMT, NULL);
  parser_addChild(A, B, 0);
  parser_addChild(A, C, 1);
  
}


if_statement(A) ::= TOK_KEY_IF condition(B) TOK_KEY_THEN statement(C) TOK_KEY_ELSE statement(D). {
  A = parser_mkNode(IF_STMT, NULL);
  parser_addChild(A, B, 0);
  parser_addChild(A, C, 1);
  parser_addChild(A, D, 2);
}


/* While Statements */
while_statement(A) ::= TOK_KEY_WHILE condition(B) TOK_KEY_DO statement(C). {
  A = parser_mkNode(WHILE_STMT, NULL);
  parser_addChild(A, B, 0);
  parser_addChild(A, C, 1);
}



write_statement(A) ::= TOK_KEY_WRITE TOK_LPAREN exp_list(B) TOK_RPAREN. {
  A = parser_mkNode(WRITE_STMT, NULL);
  parser_addChild(A, B, 0);
}


read_statement(A) ::= TOK_KEY_READ TOK_LPAREN variable_list(B) TOK_RPAREN. {
  A = parser_mkNode(READ_STMT, NULL);
  parser_addChild(A, B, 0);
}


exp_list(A) ::= expression(B). {
  A = B;
}

exp_list(A) ::= exp_list(B) TOK_COMMA expression(C). {
  A = B;
  parser_addSibling(A, C);
}

/*
 * Variable list is a list of actual variables for use specifically
 * in the read statement.
 *
 * variable(A) ::= simple_name(B). - ambiguity
 *
 * The rules involving 'simple_name' nonterminal exist because
 * reducing a simple_name to a variable creates conflicts as it
 * also reduces to a 'constant' nonterminal in the 'factor'
 * production. This creates ambiguity. To remove this ambiguity,
 * any use of the 'variable' nonterminal should take care to
 * ensure that a 'simple_name' nonterminal on its own exists
 * as an option.
 */
variable_list(A) ::= simple_name(B). {
  A = B;
}

variable_list(A) ::= variable(B). {
  A = B;
}

variable_list(A) ::= variable_list(B) TOK_COMMA variable(C). {
  A = B;
  parser_addSibling(A, C);
}

variable_list(A) ::= variable_list(B) TOK_COMMA simple_name(C). {
  A = B;
  parser_addSibling(A, C);
}



/* Const section of code */
const_sec(A) ::= const_sec(C) const_decl(D). {
  A = C;
  if (A)
    parser_addSibling(A->child[0], D);

}

const_sec(C) ::= TOK_KEY_CONST const_decl(D). {

  C = parser_mkNode(CONST_SEC, NULL);
  parser_addChild(C, D, 0);
}

const_section(A) ::= .{
  A =  NULL;
}

const_section(A) ::= const_sec(B). {
  A = B;
}

/* Constant Declarations */
const_decl(A) ::= TOK_ID(B) TOK_ASSIGN TOK_NUM(D) TOK_SEMICOLON.{
  A = parser_mkNode(CONST_DECL, NULL);

  parser_addChild(A, parser_mkNode(LITERAL, B), 0);
  parser_addChild(A, parser_mkNode(LITERAL, D), 1);
}

const_decl(A) ::= TOK_ID(B) TOK_ASSIGN TOK_STR(D) TOK_SEMICOLON.{
  A = parser_mkNode(CONST_DECL, NULL);
  parser_addChild(A, parser_mkNode(LITERAL, B), 0);
  parser_addChild(A, parser_mkNode(STRING, D), 1);
}


/* Constant values */
constant(A) ::= simple_name(B). {
  A = B;
  TreeNode_addType(A, CONSTANT);
}

constant(A) ::= TOK_NUM(B). {
  A = parser_mkNode(CONSTANT, B); 
}

constant(A) ::= TOK_STR(B). {
  A = parser_mkNode(CONSTANT, B); 
}


var_sec(A) ::= var_sec(B) var_decl(C). {
  A = B;
  if (A)
    parser_addSibling(A->child[0], C);
}

var_sec(A) ::= TOK_KEY_VAR var_decl(C). {
  A = parser_mkNode(VAR_SEC, NULL);
  parser_addChild(A, C, 0);
}


var_section(A) ::= var_sec(B). {
  A = B;
}

var_section(A) ::= . {
  A = NULL;
}

/* Variable declaration */
var_decl(A) ::= var_list(B) TOK_COLON TOK_KEY_ARRAY TOK_LPAREN constant(C) TOK_RPAREN TOK_KEY_OF TOK_KEY_INTEGER TOK_SEMICOLON. {
  A = B;
  parser_addChild(A, C, 1);
  TreeNode_addType(C, ARRAY);
  TreeNode_addType(C, TYPE);
  TreeNode_addType(C, INTEGER);
  TreeNode_addType(A, VAR_DECL);
}

var_decl(A) ::= var_list(B) TOK_COLON TOK_KEY_INTEGER(C) TOK_SEMICOLON. {
  A = B;
  
  TreeNode_t *child = parser_mkNode(TYPE, C);
  TreeNode_addType(child, INTEGER);
  
  parser_addChild(A, child, 1);
  TreeNode_addType(A, VAR_DECL);
}


/* Variable listing */
var_list(A) ::= TOK_ID(B). {
  A = parser_mkNode(VAR_LIST, NULL);
  parser_addChild(A, parser_mkNode(LITERAL, B), 0);
}

var_list(A) ::= var_list(B) TOK_COMMA TOK_ID(C). {
  A = B;
  if (A)
    parser_addSibling(A->child[0], parser_mkNode(LITERAL, C));
}


expression(A) ::= simple_expression(B). {
  A = B;
  //TreeNode_addType(A, EXP);
}

expression(A) ::= simple_expression(B) relational_operator(C) simple_expression(D). {
  A = C;
  TreeNode_addType(A, EXP);
  parser_addChild(A, B, 0);
  parser_addChild(A, D, 1);
}



simple_expression(A) ::= simple_expression(B) binary_adding_operator(C) term(D). {
  A = C;
  TreeNode_addType(A, SIMPEXP);
  parser_addChild(A, B, 0);
  parser_addChild(A, D, 1);
}

simple_expression(A) ::= unary_operator(B) term(C). {
  A = B;
  //TreeNode_addType(A, SIMPEXP);
  parser_addChild(B, C, 0);
}

simple_expression(A) ::= term(B). {
  A = B;
  //TreeNode_addType(A, SIMPEXP);
}



term(A) ::= term(B) multiply_operator(C) factor(D). {
  A = C;
  TreeNode_addType(A, TERM);
  parser_addChild(A, B, 0);
  parser_addChild(A, D, 1);
}

term(A) ::= factor(B). {
  A = B;
}


/* Constant also covers simple_name used by variable*/
factor(A) ::= constant(B). {
  A = B;
//  TreeNode_addType(A, FACTOR);
}

/*
 * grabs simple_expression ( expression ) pattern. constant
 * grabs the rest of variable
 */
factor(A) ::= variable(B). {
  A = B;
//  TreeNode_addType(A, FACTOR);
}

factor(A) ::= TOK_KEY_NOT factor(C). {
  A = C;
  TreeNode_addType(A, NOT);
}

factor(A) ::= TOK_LPAREN expression(B) TOK_RPAREN. {
  A = B;
//  TreeNode_addType(A, FACTOR);
}

/*
 * See note above regarding variable(A) ::= simple_name(B) ambiguity.
 */
variable(A) ::= simple_name(B) TOK_LPAREN expression(C) TOK_RPAREN. {
  A = B;
  TreeNode_addType(B, VARIABLE);
  parser_addChild(B, C, 0);
}


simple_name(A) ::= TOK_ID(B). {
  A = parser_mkNode(SIMP_NAME, B);
}



case_list(A) ::= case(B). {
  //A = parser_mkNode(CASE_LIST, NULL);
  //parser_addChild(A, B, 0);
  A = B;
  TreeNode_addType(B, CASE_LIST);
}

case_list(A) ::= case_list(B) TOK_SEMICOLON case(C). {
  A = B;
  parser_addSibling(A, C);
}


case(A) ::= const_list(B) TOK_COLON statement(C). {
  A = parser_mkNode(CASE, NULL);
  if (A) {
    parser_addChild(A, B, 0);
    parser_addChild(A, C, 1);
  }
}


const_list(A) ::= constant(B). {
  //A = parser_mkNode(CONST_LIST, NULL);
  //parser_addChild(A, B, 0);
  A = B;
  TreeNode_addType(B, CONST_LIST);
}

const_list(A) ::= const_list(B) TOK_COMMA constant(C). {
  A = B;
  parser_addSibling(A, C);
}


condition(A) ::= expression(B). {
  A = B;
  TreeNode_addType(A, CONDITION);
}


/* Operators for expressions */
relational_operator(A) ::= TOK_EQ(B). {
  A = parser_mkNode(RELOP, B);
}

relational_operator(A) ::= TOK_NOTEQ(B). {
  A = parser_mkNode(RELOP, B);
}

relational_operator(A) ::= TOK_LESS(B). {
  A = parser_mkNode(RELOP, B);
}

relational_operator(A) ::= TOK_GREATER(B). {
  A = parser_mkNode(RELOP, B);
}

relational_operator(A) ::= TOK_LTEQ(B). {
  A = parser_mkNode(RELOP, B);
}

relational_operator(A) ::= TOK_GTEQ(B). {
  A = parser_mkNode(RELOP, B);
}



/* Operators for simple expressions */
binary_adding_operator(A) ::= TOK_PLUS(B). {
  A = parser_mkNode(BINOP, B);                           
}

binary_adding_operator(A) ::= TOK_MINUS(B). {
  A = parser_mkNode(BINOP, B);                           
}

binary_adding_operator(A) ::= TOK_KEY_OR(B). {
  A = parser_mkNode(BINOP, B);                          
}


unary_operator(A) ::= TOK_PLUS(B). {
  A = parser_mkNode(UNARYOP, B);
}

unary_operator(A) ::= TOK_MINUS(B). {
  A = parser_mkNode(UNARYOP, B);
}



/* Operators for terms */
multiply_operator(A) ::= TOK_STAR(B). {
  A = parser_mkNode(MULOP, B);
}

multiply_operator(A) ::= TOK_KEY_DIV(B). {
  A = parser_mkNode(MULOP, B);
}

multiply_operator(A) ::= TOK_KEY_MOD(B). {
  A = parser_mkNode(MULOP, B);
}

multiply_operator(A) ::= TOK_KEY_AND(B). {
  A = parser_mkNode(MULOP, B);
}

multiply_operator(A) ::= TOK_KEY_SHR(B). {
  A = parser_mkNode(MULOP, B);
}

multiply_operator(A) ::= TOK_KEY_SHL(B). {
  A = parser_mkNode(MULOP, B);
}

