/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Semantic Analysis Functions
 */
#include <stdio.h>
#include <stdarg.h>

#include "analyze.h"
#include "parser.h"
#include "parserHelper.h"
#include "defines.h"
#include "bittree.h"

#define ERROR_OUT stderr
/*
 * Some prime number to determine how large the initial size
 * of a symbol table to use.
 */
#define SYMTAB_BASE_SIZE 71

#define RODATA_PREFIX "sc"
#define RODATA_LITERAL "literal"
#define RODATA_KEY_LEN 128

#define SEMANTIC_ERR_HEADER ("\nSemantic Error [line %d]: ")
#define SEMANTIC_WARN_HEADER ("\nSemantic Warning [line %d]:\n\t")

#define IDENTIFIER_SIZE 256

/*
 * Create a filter for checking node types that will have an
 * expected return value;
 */
#define RETURN_TYPES_FILTER                                             \
  ( NODETYPE_BIT(VARIABLE) | NODETYPE_BIT(ARRAY) |                      \
    NODETYPE_BIT(UNARYOP) | NODETYPE_BIT(FACTOR) | NODETYPE_BIT(TERM) | \
    NODETYPE_BIT(SIMPEXP) | NODETYPE_BIT(EXP) | NODETYPE_BIT(ASSIGN_STMT) | \
    NODETYPE_BIT(IF_STMT) | NODETYPE_BIT(WHILE_STMT) | NODETYPE_BIT(CASE_STMT) | \
    NODETYPE_BIT(READ_STMT) | NODETYPE_BIT(WRITE_STMT)                  \
  )


//before adding, check to see if it already exists in the
//current scope
#define REDECLARED_CHECK(node, id) do {                     \
    Symbol_t *found = SymTable_find(currentScope, id);    \
    if (found) {                                            \
      semanticMsg(REDECLARED_ID, node, key);                \
      return -1;                                            \
    }                                                       \
  } while (0)                                               


//Since Booleans are integers, it is possible to add/multiply a
//Boolean to an integer. It is also possible for integers to
//act as integers (0 false, anything else true)
#define CHECK_FOR_BOOL_INT(msg, type, node) do {                         \
    if(isBoolOrInt(msg, type, node))                                    \
      return -1;                                                        \
  } while (0) 


//Check for null statements in else condition.
//Visually, a null else statement can be misleading,
//and should be avoided.
#define CHECK_NULL_ELSE_STMT(parent) do {                               \
    TreeNode_t *elseClause = TreeNode_getChild(parent, 2),              \
      *condition = TreeNode_getChild(parent, 0);                        \
    if (!elseClause)                                                    \
      return 0;                                                         \
    if (TreeNode_hasType(elseClause, NULL_STMT))                        \
      semanticWarning(ELSE_NULL_STMT, condition);                       \
  } while (0)



#define LVAL_CHECK_FOR_BOOL_INT(type, node) \
  CHECK_FOR_BOOL_INT(INVALID_L_TYPE, type, node)

#define LVAL_EXPECTS_BOOL_INT(type, node) \
  CHECK_FOR_BOOL_INT(UNEXPECTED_L_TYPE, type, node)

#define RVAL_EXPECTS_BOOL_INT(type, node) \
  CHECK_FOR_BOOL_INT(UNEXPECTED_R_TYPE, type, node)

#define OR_DELIM (" or ")
#define AND_DELIM (" and ")

//if a node has type of NOT, set the node
//return type to Boolean
#define EVAL_NOT(node) do {                        \
  if (TreeNode_hasType(node, NOT))                 \
    TreeNode_setReturnType(node, RETURN_BOOL);     \
} while (0)

typedef enum {
  ARRY_INDEX_TYPE,
  ARRY_INIT_SIZE,
} SEMANTIC_ARRAY_ERRORS;

typedef enum {
  ELSE_NULL_STMT,
} SEMANTIC_WARNINGS;


//These strings match with constants found in analyze.h
static const char *SEMANTIC_ERROR_MSGS[] = {
  "No Errors\n",
  "Redeclared identifier '%s'.\n",
  "Use of undeclared identifier '%s'.\n",
  "Invalid array declaration: ",
  "Unexpected r-value type: \n",
  "Invalid l-value type: ",
  "Unexpected l-value type: \n",
  "Invalid conditional type: ",
  "Array '%s' used without index. An index must be specified\n",
  "Index out of bounds: negative index '%d' unsupported.\n",
  "Index out of bounds: index '%d' greater than '%d'.\n",
  "Duplicate case detected. All cases must be unique.\n",
  "Index attempt on non-array variable.\n",
  "'Not' value assignment.\n",
};

static const char *ARRAY_ERR_MSGS[] = {
  "Expected 'Constant Integer' index type; found type: ",
  "Invalid array size '%d', must be greater than 0.\n"
};

static const char *TYPE_ERR_MSGS = "Expected type(s) %s; found '%s' instead.\n";



static const char *SEMANTIC_WARN_MSGS[] = {
  "'else' condition using implicit Null Statment.",
};


//keep track of the current semantic analysis status
static SemanticErrors errors = NONE;

//global pointer to the current scopes symbol table
static SymTable_t *currentScope = NULL;

static SymTable_t *rodata = NULL;


static char *makeLiteralKey(void) {
  static int literalCount = 0;
  static char literalBuf[RODATA_KEY_LEN];


  snprintf(literalBuf, RODATA_KEY_LEN, "%s%d", RODATA_LITERAL, literalCount++);
  return literalBuf;
}


/*==============================================================================
 * Error and Warning Message functions.
 *
 *============================================================================*/

/*
 * makeExpectedList:
 *  Generates a list of expected results as a string.
 *
 * Arguments:
 *  delim: String delimiter to separate suggestions with.
 *  outputBuf: Destination to write generated list to.
 *  outputLen: Size of destination in bytes.
 *  typeText: Array of strings representing the available type suggestions.
 *  count: Number of elements that are to be expected
 *  types: integer array of expected type values that map to the typeText array.
 */
static void makeExpectedList(const char *delim, char *outputBuf, size_t outputLen,
           const char **typeText, int count, int *types) {

  memset(outputBuf, 0, sizeof(outputLen));

  int val = 0;
  for (int i = 0; i < count; i++) {
    val = types[i];
    SAFECAT(outputBuf, "'", outputLen);
    SAFECAT(outputBuf, typeText[val], outputLen);
    SAFECAT(outputBuf, "'", outputLen);
    if (i < count - 1)
      SAFECAT(outputBuf, delim, outputLen);
  }
  SAFECAT(outputBuf, "\0", outputLen);
  
}


//See semanticMsg below, accepts va_list instead of ...
static void vSemanticMsg(SemanticErrors errorID, TreeNode_t *node, va_list args) {
 
  int lineNum = -1;
  char identifier[IDENTIFIER_SIZE];
  size_t idLen = sizeof(identifier) - 1;
  memset(identifier, 0, idLen);

  Symbol_t *ref = TreeNode_getSymbolRef(node);
  
  if (node->token) {
    lineNum = node->token->line;
    //check if node has a reference to a symbol first
    if (ref) 
      strncpy(identifier, ref->key, idLen);
    
    //otherwise, probe the node
    else if (node->token->type == TOK_STR) 
      strncpy(identifier, node->token->lexeme.string, idLen);
    else if (node->token->type == TOK_NUM)
      snprintf(identifier, idLen, "%d", node->token->lexeme.value);
  }
 
  fprintf(ERROR_OUT, SEMANTIC_ERR_HEADER, lineNum);

  if (strlen(identifier) > 0)
    fprintf(ERROR_OUT, "value: \"%s\"\n\t", identifier);
  else
    fprintf(ERROR_OUT, "\n\t");
  
  vfprintf(ERROR_OUT, SEMANTIC_ERROR_MSGS[errorID], args);
  
  errors = errorID;
}

//General semantic error message display
//prints out the line number, and offending value if available,
//then prints out a message describing the error.
static void semanticMsg(SemanticErrors errorID, TreeNode_t *node, ...) {
  va_list args;
  va_start(args, node);
  vSemanticMsg(errorID, node, args);
  va_end(args);
}

//Semantic error messages for dealing with array declarations
static void semanticErrArray(SEMANTIC_ARRAY_ERRORS errorID, Symbol_t *symbol, TreeNode_t *node, ...) {

  semanticMsg(INVALID_ARRAY_DECL, node);

  fprintf(ERROR_OUT, "\n\t");
  va_list args;
  va_start(args, node);
  vfprintf(ERROR_OUT, ARRAY_ERR_MSGS[errorID], args);
  va_end(args);

  switch (errorID) {
  case ARRY_INDEX_TYPE:
    if (Symbol_hasType(symbol, SYMTYPE_VARIABLE))
      fprintf(ERROR_OUT, "'%s'\n", SYM_TYPE_TEXT[SYMTYPE_VARIABLE]);
    else if (Symbol_hasType(symbol, SYMTYPE_STR)) 
      fprintf(ERROR_OUT, "'%s %s'\n", SYM_TYPE_TEXT[SYMTYPE_CONSTANT], SYM_TYPE_TEXT[SYMTYPE_STR]);
    
    
    break;
  case ARRY_INIT_SIZE:
    break;
    
  } 
}


//Semantic error messages for expected return types for:
//  -multiply operators
//  -relational operators
//  -unary operators
//  -binary adding operators
//  -expressions, and conditions
static void semanticErrTypes(SemanticErrors errorID, NodeReturnType found,
           TreeNode_t *node, int count, ...) {

  semanticMsg(errorID, node);
  char expectedBuf[IDENTIFIER_SIZE];
  size_t expectedBufLen = sizeof(expectedBuf);

  int types[count];
  va_list expectedTypes;

  //populate expected types array
  va_start(expectedTypes, count);
  for (int i = 0; i < count; i++) {
    int val = va_arg(expectedTypes, int);
    types[i] = val;
  }
  va_end(expectedTypes);

  makeExpectedList(OR_DELIM, expectedBuf, expectedBufLen, NODE_RETURNTYPE_TEXT, count, types);
  fprintf(ERROR_OUT, TYPE_ERR_MSGS, expectedBuf, NODE_RETURNTYPE_TEXT[found]);
}

/* Display message for expected node types
 * Errors of these types are technically syntax errors, however, during
 * the parsing, identifier types (variable or constant) are ambiguous and
 * are resolved in the semantic analysis.
 */
static void semanticErrNodeType(SemanticErrors errorID, NodeType found, TreeNode_t *node, int count, ...) {

  semanticMsg(errorID, node);

  char expectedBuf[IDENTIFIER_SIZE];
  size_t expectedBufLen = sizeof(expectedBuf);

  //populate expected types list, exclude all types
  //that have been already found.
  //For example, if a Variable Integer is expected and
  //a Constant Integer was found, it will only suggest
  //the 'Variable' type
  int types[count];
  va_list expectedTypes;
  
  va_start(expectedTypes, count);

  //remove any expected types that already exist in the found types
  int newCount = 0;
  for (int i = 0; i < count; i++) {
    int val = va_arg(expectedTypes, int);

    if (!(found & NODETYPE_BIT(val)))
      types[newCount++] = val;
    else
      //strip out the found expected types
      found &= ~NODETYPE_BIT(val);
  }
  va_end(expectedTypes);

  //generate string list of expected types
  makeExpectedList(AND_DELIM, expectedBuf, expectedBufLen, NODE_TYPE_TEXT, newCount, types);

  //then find the first type that is unexpected
  NodeType firstFound = 0;
  for (int i = NODETYPE_BITS_COUNT - 1; i >= 0 && firstFound == 0; i--) {
    if (found & NODETYPE_BIT(i))
      firstFound = i;
  }
  
  fprintf(ERROR_OUT, TYPE_ERR_MSGS, expectedBuf, NODE_TYPE_TEXT[firstFound]);
}


/*
 * Semantic Warnings do not halt the analysis, but just provides some
 * feedback on clarifying code.
 */
static void semanticWarning(SEMANTIC_WARNINGS warningID, TreeNode_t *node) {
  int lineNum = -1;
  
  if (node->token) 
    lineNum = node->token->line;
 
  fprintf(ERROR_OUT, SEMANTIC_WARN_HEADER, lineNum); 
  fprintf(ERROR_OUT, "%s", SEMANTIC_WARN_MSGS[warningID]);
}

static int isBoolOrInt(SemanticErrors msg, NodeReturnType type, TreeNode_t *node) {
  if (type != RETURN_INT && type != RETURN_BOOL) {
    semanticErrTypes(msg, type, node, 2, RETURN_INT, RETURN_BOOL);
    return -1;
  }
  return 0;
}
/*==============================================================================
 * Symbol Table Helper functions.
 *
 *============================================================================*/


/* 
 * When entering a scope creates a new symbol table,
 * sets the current symbol table to the new symbol table's
 * parent, and then updates the current symbol table to this
 * newly created table.
 */
static int enterScope(TreeNode_t *node) {

  SymTable_t *newScope = SymTable_init(SYMTAB_BASE_SIZE);
  if (!newScope)
    return -1;

  
  //set new scopes parent to the current scope
  SymTable_addParent(newScope, currentScope);
  //update current scope to the new scope
  currentScope = newScope;
  TreeNode_setSymTable(node, currentScope);
  return 0;
}

/*
 * Exit the current scope we are in, and go to the parent scope
 */
static void exitScope(void) {
  currentScope = currentScope->parent;
}


/*
 * Lookup a symbol in current scope. If symbol
 * isn't found, searches the parent scopes
 * until no more parent scopes exist.
 */
static Symbol_t *lookupScope(char *key) {

  return SymTable_findAll(currentScope, key, NULL);
}


char *getConstRodataKey(SymTable_t *scope, char *inKey) {
  
  static char rodatakey[RODATA_KEY_LEN];
  snprintf(rodatakey, RODATA_KEY_LEN, "%s%d_%s", RODATA_PREFIX, scope->id, inKey);
  return rodatakey;
}

//Read only data table will only hold string constants
static Symbol_t *addRoData(SymTable_t *scope, char *key, symdata data, SymbolType type) {

  char *newKey = NULL;
  if (key)
    newKey = getConstRodataKey(scope, key);
  //no key exists, so generate one
  else
    newKey = makeLiteralKey();

  //since the symbol table doesn't make copies of keys,
  //we need to allocate the generated key here.
  char *allocdKey = calloc(1, strlen(newKey) + 1);
  if (!allocdKey) {
    fprintf(stderr, "Error allocating key for rodata entry: %s\n", newKey);
    return NULL;
    
  }
  
  strcpy(allocdKey, newKey);
  Symbol_t  *symbol = Symbol_create(allocdKey, data, type);
  SymTable_add(rodata, symbol);
  return symbol;
}

static Symbol_t *findRoData(SymTable_t *scope, char *key) {

  SymTable_t *thisScope = scope;
  Symbol_t *entry = NULL;
  while (thisScope && !entry) {
    char *newKey = getConstRodataKey(thisScope, key);
    entry = SymTable_find(rodata, newKey);
    thisScope = thisScope->parent;
  }

  return entry;
}



/*
 * Add a constant to the current scopes
 * symbol table.
 */
static int addConstant(TreeNode_t *node) {
  char *key = NULL;
  symdata data;
  SymbolType type = SYMTYPE_BIT(SYMTYPE_CONSTANT);

  /*
   * Constant declaration nodes have have child 0 set to
   * the identifier it uses, and child 1 to the value
   * it contains.
   */
  TreeNode_t *identifier = node->child[0],
    *value = node->child[1];
  
  key = identifier->token->lexeme.string;
  
  if (value->token->type == TOK_NUM) {
    data = (symdata)value->token->lexeme;
    type |= SYMTYPE_BIT(SYMTYPE_INT);
  } else {
    data = (symdata)value->token->lexeme;
    type |= SYMTYPE_BIT(SYMTYPE_STR);

    //add string constants to .rodata
    if (!addRoData(currentScope, key, data, type))
      return -1;
  }
  
  
  //before adding, check to see if it already exists in the
  //current scope
  REDECLARED_CHECK(identifier, key);
  
  Symbol_t  *symbol = Symbol_create(key, data, type);
  SymTable_add(currentScope, symbol);
  return 0;
}

/*
 * Add a declared variable to the current scopes
 * symbol table. Also performs type checking
 * on array declarations to ensure a valid
 * array size is used.
 */
static int addVariable(TreeNode_t *node) {

  char *key = NULL;
  symdata data = (symdata)0;
  SymbolType type = 0;
  /*
   * Variable declaration nodes have child 0 set to
   * the identifier it uses, and child 1 to the
   * type it is declared as.
   */
  TreeNode_t *identifier = node->child[0],
    *declareType = node->child[1];

  NodeType nodeType = declareType->type;
  NodeType arrayIndex = NODETYPE_BIT(CONSTANT) | NODETYPE_BIT(INTEGER);
  
  //First check if variable is an array
  if (TreeNode_hasType(declareType, ARRAY)) {

    int indexSize = -1;

    //either constant or variable lookup
    if (nodeType & arrayIndex && TreeNode_hasType(declareType, SIMP_NAME)) {
      //if we have an array size using a constant identifier
      //check to see if that identifier has been declared first
      char *key = declareType->token->lexeme.string;
      Symbol_t *constant = lookupScope(key);
      
      if (constant) {
        data = (symdata)declareType->token->lexeme;
        type |= SYMTYPE_BIT(SYMTYPE_CONST_REF);
        
        //check that the constant used for the array index is an integer
        if (!Symbol_hasType(constant, SYMTYPE_INT) ||
            !Symbol_hasType(constant, SYMTYPE_CONSTANT)) {
          //ERROR
          semanticErrArray(ARRY_INDEX_TYPE, constant, declareType);
          return -1;
        }

        //save the index size referenced by the constant for verification
        indexSize = constant->data.value;
       
      } else {
        //display error message for undeclared variable
        semanticMsg(UNDECLARED_ID, node, key);
        return -1;
      }  
    }
    //verifying the numerical representation of the index size
    if (nodeType & arrayIndex) {
      //value is not referring to an identifier
      //check that size integer is greater than 0
      int size = indexSize;

      //if the size has not been a referenced by a constant
      //identifier, get the numerical value
      if (size < 0)
        size = declareType->token->lexeme.value;
      
      if (size <= 0) {
        //ERROR
        semanticErrArray(ARRY_INIT_SIZE, NULL, declareType, size);
        return -1;
      }
      //if the size isn't already being referenced by a constant
      //set it to the integer value that it holds
      //if (!data)
      //  data = (void *)&declareType->token->lexeme.value;
      if (!data.string)
        data = (symdata)declareType->token->lexeme;
    }
      
    type |= SYMTYPE_BIT(SYMTYPE_ARRAY);
  }
    
  
  //if integer, we just need to mark it so in the table
  if (TreeNode_hasType(declareType, INTEGER)) {
      type |= SYMTYPE_BIT(SYMTYPE_INT);
  }
  
  //otherwise, we've just got a variable
  type |= SYMTYPE_BIT(SYMTYPE_VARIABLE);
  
  
  //variables can be defined in lists, so we need to make sure
  //we loop through all of them in the list and define them as the same type
  do {
    key = identifier->token->lexeme.string;

    //before adding, check to see if it already exists in the
    //current scope
    REDECLARED_CHECK(identifier, key);

    //continue adding variable declarations
    Symbol_t  *symbol = Symbol_create(key, data, type);
    SymTable_add(currentScope, symbol);

    //store where variable would be on the stack
    SymTable_addStackVar(currentScope, symbol);
    identifier = identifier->sibling;
    
  } while (identifier != NULL);

  return 0;
}



//add symbol to current symbol table
static int addSymbol(TreeNode_t *node) {

  if (currentScope == NULL) {
    fprintf(stderr, "CURRENT SCOPE IS NULL\n");
    return 0;
  }

  if (!node)
    return 0;
  
  //when adding values to the symbol table, it must be done through
  //a variable or constant declaration
  if (TreeNode_hasType(node, CONST_DECL))
    return addConstant(node);
  
  else if (TreeNode_hasType(node, VAR_DECL))
    return addVariable(node);

  return 0;
}




static int resolveLiterals(TreeNode_t *node) {
  
  if (node->token->type == TOK_NUM) {
    TreeNode_addType(node, INTEGER);
    TreeNode_setReturnType(node, RETURN_INT);
    EVAL_NOT(node);
    return 0;
  }
  //otherwise a literal string
  TreeNode_setReturnType(node, RETURN_STR);
  TreeNode_addType(node, STRING);
  
  //add string literal to rodata table
  Symbol_t *entry = addRoData(currentScope, NULL, (symdata)node->token->lexeme,
                              SYMTYPE_STR(SYMTYPE_CONSTANT));
  
  if (!entry)
    return -1;
  
  //make sure this node can reference the rodata entry
  TreeNode_setSymbolRef(node, entry);
  return 0;
}

/*
 * Solves any ambiguities in nodes that are
 * refering to a variable, or constant identifier
 *
 * By default, when the AST is created, any identifier outside 
 * of a variable declaration is stored as a constant
 */
int resolveVariables(TreeNode_t *node) {
  
  //not an identifier, but a constant literal
  if (!TreeNode_hasType(node, SIMP_NAME))
    return resolveLiterals(node);
    
  /*
   * If we are dealing with a constant node that
   * has an identifier, do a look up in the symbol table.
   */
  char *identifier = node->token->lexeme.string;
  Symbol_t *symbol = lookupScope(identifier);
    
  //identifier was not declared before its use....
  if (!symbol) {
    //ERROR
    semanticMsg(UNDECLARED_ID, node, identifier);
    return -1;
  }

  TreeNode_setSymbolRef(node, symbol);
  
  //otherwise, entry exists, check its type
  if (Symbol_hasType(symbol, SYMTYPE_VARIABLE)) {
    //declared as a variable, modify the node in the AST to reflect
    //the proper type
    TreeNode_rmType(node, CONSTANT);
    TreeNode_rmType(node, SIMP_NAME);
    TreeNode_addType(node, VARIABLE);
    TreeNode_setReturnType(node, RETURN_INT);
    
    if (Symbol_hasType(symbol, SYMTYPE_ARRAY))
      TreeNode_addType(node, ARRAY);
    
   
  } else {
    //not a variable, but a constant, set the return type
    if (Symbol_hasType(symbol, SYMTYPE_INT)) {
      TreeNode_setReturnType(node, RETURN_INT);
      TreeNode_addType(node, INTEGER);
    } else {
      TreeNode_setReturnType(node, RETURN_STR);
      TreeNode_addType(node, STRING);

      //make declared constants refer to their rodata entry
      Symbol_t *rentry = findRoData(currentScope, symbol->key);
      TreeNode_setSymbolRef(node, rentry);
    }
    TreeNode_rmType(node, SIMP_NAME);
    TreeNode_addType(node, CONSTANT);
  }
  
  if (TreeNode_hasType(node, INTEGER) || TreeNode_hasType(node, VARIABLE))
    EVAL_NOT(node);
  
  return 0;
}

/*==============================================================================
 *
 * Type checking and node return type evaluation.
 *
 * As nodes are type checked, their return type is set for parent nodes
 * to utilize in their own type checking.
 *============================================================================*/

/*
 * Type check arrays that are being used
 * outside of declarations. Checks that
 * an identifier refering to an array is being
 * indexed with a valid value. 
 */
static int typecheck_Array(TreeNode_t *node) {

  //check the array index value to ensure its an integer
  TreeNode_t *first = TreeNode_getChild(node, 0);

  //make sure if the array is being used, it has an index specified
  if (!first) {
    semanticMsg(ARRAY_MISSING_INDEX, node, node->token->lexeme.string);
    return -1;
  }

  NodeReturnType type = TreeNode_getReturnType(first);
  
  //make sure the first child is an integer or Boolean
  LVAL_CHECK_FOR_BOOL_INT(type, first);

  //if the array index is a variable, we cannot
  //determine the bounds
  if (TreeNode_hasType(first, VARIABLE) || TreeNode_hasType(first, EXP) ||
      TreeNode_hasType(first, SIMPEXP)) {
    //indexing into an array returns a type of integer
    TreeNode_setReturnType(node, RETURN_INT);
    return 0;
  }

  //get the array size to use later
  Symbol_t *arraySizeSymbol = Symbol_getArraySizeEntry(currentScope, TreeNode_getSymbolRef(node));
  int arraySize = arraySizeSymbol->data.value;
    
  //otherwise, check if the index value has a symbol table entry
  //The presence of this symbol table entry indicates it using
  //a declared constant value
  Symbol_t *childEntry = TreeNode_getSymbolRef(first);
  if (childEntry) {
    //since constants cannot be declared as negative values
    //we just need to test if it goes over the array bounds
    int index = childEntry->data.value;
    
    if (index >= arraySize) {
      //ERROR
      semanticMsg(ARRAY_OVER_BOUNDS, node, index, arraySize);
      return -1;
    }

    //otherwise, its good
    
    //indexing into an array returns a type of integer
    TreeNode_setReturnType(node, RETURN_INT);
    return 0;
  }

  //if there is no symbol table entry, then we need to probe
  //the node for information
  if (TreeNode_hasType(first, UNARYOP)) {
    //if the first child of an array index is TOK_MINUS
    //we can't be sure if the index will actually be negative
    //unless we know what the Unary minus node is the parent of.
    //If the child of the unary minus is a variable, index
    //can't be determined, but if its a constant or literal,
    //then we have a negative index

      
    TreeNode_t *indexNum = TreeNode_getChild(first, 0);
    if (TreeNode_hasType(indexNum, VARIABLE)) {
      //can't determine run time behaviour
      TreeNode_setReturnType(node, RETURN_INT);
      return 0;
    }

    //So its not a variable, check if its in the symbol table
    //and if not, probe the node token for the value
    //Then we can at least report the incorrect value that is
    //being used for indexing.
    int index = -1;

    Symbol_t *indexRef = TreeNode_getSymbolRef(indexNum);
    if (indexRef)
      index = indexRef->data.value;
    else 
      index = indexNum->token->lexeme.value;
      
    if (first->token->type == TOK_MINUS) {    
      semanticMsg(ARRAY_UNDER_BOUNDS, first, -index);
      return -1;
    }
    //otherwise, make sure its not over bounds
    if (index >= arraySize) {
      semanticMsg(ARRAY_OVER_BOUNDS, node, index, arraySize);
      return -1;
    }
  }
  
  //Alright, so the value of the array index child is neither in the symbol
  //table, and its not a value with a unary operator, nor is it a variable;
  //we can just check the token literal value, if its an integer
  if (type == RETURN_INT) {
    int index = first->token->lexeme.value;
    if (index >= arraySize) {
      semanticMsg(ARRAY_OVER_BOUNDS, node, index, arraySize);
      return -1;
    }
  } else {
    semanticErrTypes(INVALID_L_TYPE, type, first, 1, RETURN_INT);
    return -1;
  }

  //All checks passed

  //indexing into an array returns a type of integer
  TreeNode_setReturnType(node, RETURN_INT);
  return 0;
}

static int typecheck_Variable(TreeNode_t *node) {

  //if a child exists for a variable, then its being indexed like an
  //array
  if (TreeNode_getChild(node, 0)) {
    semanticMsg(VAR_INDEXING, node);
    return -1;
  }

  return 0;
}


static int typecheck_Unary(TreeNode_t *node) {
  
  //for unary op, just set the type to the same type as the
  //first child. 
  TreeNode_t *first = TreeNode_getChild(node, 0);
  NodeReturnType type = TreeNode_getReturnType(first);

  
  //make sure the first child is an integer or Boolean
  RVAL_EXPECTS_BOOL_INT(type, first);

  //set unary node return type to child type
  //TreeNode_setReturnType(node, RETURN_BOOL);
  TreeNode_setReturnType(node, type);
  return 0;
}

static int typecheck_MulOp(TreeNode_t *node) {
  
  TreeNode_t *firstChild = TreeNode_getChild(node, 0),
    *secondChild = TreeNode_getChild(node, 1);

  NodeReturnType firstType = TreeNode_getReturnType(firstChild),
    secondType = TreeNode_getReturnType(secondChild);

  //make sure the first child has an integer or Boolean return
  LVAL_EXPECTS_BOOL_INT(firstType, firstChild);
  //make sure the next child has an integer or Boolean return
  RVAL_EXPECTS_BOOL_INT(secondType, secondChild);
  
  //this type of operator returns an integer
  if (node->token->type == TOK_KEY_AND)
    TreeNode_setReturnType(node, RETURN_BOOL);
  else
    TreeNode_setReturnType(node, RETURN_INT);
  return 0;
}

static int typecheck_RelationalOp(TreeNode_t *node) {
  
  TreeNode_t *firstChild = TreeNode_getChild(node, 0),
    *secondChild = TreeNode_getChild(node, 1);

  NodeReturnType firstType = TreeNode_getReturnType(firstChild),
    secondType = TreeNode_getReturnType(secondChild);

  //make sure the first child has an integer or Boolean return
  LVAL_EXPECTS_BOOL_INT(firstType, firstChild);
  //make sure the next child has an integer or Boolean return
  RVAL_EXPECTS_BOOL_INT(secondType, secondChild);

  //make sure both types are the same
  if (secondType != firstType) {
    semanticErrTypes(UNEXPECTED_R_TYPE, secondType, secondChild, 1, firstType);
    return -1;
  }
  
  //This operator returns a Boolean
  TreeNode_setReturnType(node, RETURN_BOOL);
  return 0;
}

static int typecheck_BinOp(TreeNode_t *node) {

  TreeNode_t *firstChild = TreeNode_getChild(node, 0),
    *secondChild = TreeNode_getChild(node, 1);

  NodeReturnType firstType = TreeNode_getReturnType(firstChild),
    secondType = TreeNode_getReturnType(secondChild);

  //make sure the first child has an integer or Boolean return
  LVAL_EXPECTS_BOOL_INT(firstType, firstChild);
  //make sure the next child has an integer or Boolean return
  RVAL_EXPECTS_BOOL_INT(secondType, secondChild);

  //This operator returns an integer, unless its or
  if (node->token->type == TOK_KEY_OR)
    TreeNode_setReturnType(node, RETURN_BOOL);
  else
    TreeNode_setReturnType(node, RETURN_INT);
  return 0;
}


static int typecheck_Factor(TreeNode_t *node) {

  NodeReturnType type = TreeNode_getReturnType(node);
  
  //make sure the first child has an integer or Boolean return
  RVAL_EXPECTS_BOOL_INT(type, node);
  return 0;
}

static int typecheck_Assignment(TreeNode_t *node) {

  TreeNode_t *firstChild = TreeNode_getChild(node, 0),
    *secondChild = TreeNode_getChild(node, 1);

  NodeReturnType firstType = TreeNode_getReturnType(firstChild),
    secondType = TreeNode_getReturnType(secondChild);


  //first check left hand value, make sure its an integer variable
  if (!TreeNode_hasType(firstChild, VARIABLE)) {
    //first check if the assignment is some string
    NodeType foundType = firstChild->type;
    //this is actually a syntax error
    semanticErrNodeType(INVALID_L_TYPE, foundType, firstChild, 2, VARIABLE, INTEGER);
    return -1;
  }
  
  if (firstType != RETURN_INT && firstType != RETURN_BOOL) {                      
    semanticErrTypes(INVALID_L_TYPE, firstType, firstChild, 2, RETURN_INT, RETURN_BOOL);
    return -1;
  }

  //Catch any errors with not'd values
  if (TreeNode_hasType(secondChild, NOT)) {
    semanticErrTypes(NOT_VALUE, secondType, secondChild, 1, RETURN_INT);
    return -1;
  }
  
  //next, check the r-value for proper type
  //must be an integer, cannot assign a Boolean value
  //    RVAL_EXPECTS_BOOL_INT(secondType, secondChild);  
  if (secondType != RETURN_INT) {
    semanticErrTypes(UNEXPECTED_R_TYPE, secondType, secondChild, 1, RETURN_INT);
    return -1;
  }

  
  return 0;
}

/*
 * Conditions always depend on Boolean return types
 */
static int typecheck_Condition(TreeNode_t *node) {

  //Conditions are just expression nodes decorated with
  //a 'CONDITION' type. Check the nodes return type to
  //ensure it evaluates to a Boolean.
  NodeReturnType type = TreeNode_getReturnType(node);

  if (type != RETURN_BOOL) {
    semanticErrTypes(INVALID_CONDITION, type, node, 1, RETURN_BOOL);
    return -1;
  }
  
  return 0;
}


static int typecheck_IfStmt(TreeNode_t *node) {

  TreeNode_t *condition = TreeNode_getChild(node, 0);

  //make sure condition returns a Boolean result
  int conditionStatus = typecheck_Condition(condition);
  if (conditionStatus)
    return conditionStatus;


  CHECK_NULL_ELSE_STMT(node);
  
  return 0;
}

static int typecheck_WhileStmt(TreeNode_t *node) {

  TreeNode_t *condition = TreeNode_getChild(node, 0);

  //make sure condition returns a Boolean result
  return typecheck_Condition(condition);
}

static int typecheck_CaseStmt(TreeNode_t *node) {

  TreeNode_t *exp = TreeNode_getChild(node, 0);
  NodeReturnType expType = TreeNode_getReturnType(exp);

  //make sure the expression returns a bool or integer
  LVAL_EXPECTS_BOOL_INT(expType, exp);

  //go through each case and the constants for each case
  //and make sure that 1) the constants are integer values, and
  //2)no duplicate constants exist for cases.
  BitTreeNode_t *bitTree = BitTreeNode_New();
  if (!bitTree)
    return -1;

  int maxCase = 0;
  
  //Go through each case
  TreeNode_t *cases = TreeNode_getChild(node, 1);
  do {

    //for each case, go through the constants
    TreeNode_t *constants = TreeNode_getChild(cases, 0);
    do {
      
      //make sure constant is an integer first
      if(isBoolOrInt(UNEXPECTED_L_TYPE, TreeNode_getReturnType(constants), constants)) {
        BitTreeNode_Destroy(bitTree);
        return -1;
      }
      
      //constant indexes cannot be negative
      int hashIndex = -1;
    
      if (constants->token->type == TOK_ID) {
        //used a constant identifier, look it up and get the value
        Symbol_t *value = lookupScope(constants->token->lexeme.string);
        hashIndex = value->data.value; 
      } else
        hashIndex = constants->token->lexeme.value;

      //check if value is a duplicate
      if (BitTreeNode_AddBitPattern(bitTree, (intptr_t)hashIndex)) {
        semanticMsg(DUPLICATE_CASE, constants);
        BitTreeNode_Destroy(bitTree);
        return -1;
      }
      //otherwise it is unique      
      //if this is a const list, check all constants in it

      //keep track of the largest case value
      if (hashIndex > maxCase)
	maxCase = hashIndex;
      
    } while ((constants = constants->sibling));


    //move onto next case
  } while ((cases = cases->sibling)); 

  BitTreeNode_Destroy(bitTree);

  //add number of cases to node
  TreeNode_setArgCount(node, maxCase);

  //check for null statement in else condition
  CHECK_NULL_ELSE_STMT(node);
 
  //all checks passed
  return 0;
}

static int typecheck_ReadStmt(TreeNode_t *node) {

  TreeNode_t *arg = TreeNode_getChild(node, 0);
  int argc = 0;
  
  do {
    //make sure the arguments given are all variables
    if (!TreeNode_hasType(arg, VARIABLE)) {
      semanticErrNodeType(UNEXPECTED_R_TYPE, arg->type, arg, 2, VARIABLE, INTEGER);
      return -1;
    }

    //keep track of the number of arguments
    argc++;
  } while((arg = arg->sibling));

  TreeNode_setArgCount(node, argc);  
  return 0;
}

static int typecheck_WriteStmt(TreeNode_t *node) {

  TreeNode_t *arg = TreeNode_getChild(node, 0);
  int argc = 0;

  //write statement doesn't care what data types are
  //given, it can handle consts, variables, Booleans,
  //and strings.
  do {
    //count the number of arguments
    argc++;
  } while((arg = arg->sibling));

  //store argument count in node
  TreeNode_setArgCount(node, argc);
  return 0;
}

/*
 * Determines and checks the return type for each node.
 * Nodes types that have a return type include:
 *  -condition nodes
 *  -simple expressions/expression nodes
 *  -term nodes
 *  -factor nodes
 *
 * These nodes are singled out for having return types
 * as they are typically used as/in operations that
 * depend on the type of their child nodes to complete
 * without error.
 */
int resolveReturnTypes(TreeNode_t *node) {

  //Type cast 'type' to an integer so
  //the compile stops complaining about values
  //not declared in the NodeType enum.
  //Filter out the node types that don't have an expected return value
  unsigned long long type = node->type & RETURN_TYPES_FILTER;

  switch (type) {
  case NODETYPE_BIT(ARRAY) | NODETYPE_BIT(VARIABLE):
    return typecheck_Array(node);
    
  case NODETYPE_BIT(VARIABLE):
    return typecheck_Variable(node);
    
  case NODETYPE_BIT(UNARYOP):
    return typecheck_Unary(node);

  case NODETYPE_BIT(FACTOR):
    return typecheck_Factor(node);

  case NODETYPE_BIT(TERM):
    return typecheck_MulOp(node);

  case NODETYPE_BIT(SIMPEXP):
    return typecheck_BinOp(node);

  case NODETYPE_BIT(EXP):
    return typecheck_RelationalOp(node);
    
  case NODETYPE_BIT(ASSIGN_STMT):
    return typecheck_Assignment(node);

  case NODETYPE_BIT(IF_STMT):
    return typecheck_IfStmt(node);

  case NODETYPE_BIT(WHILE_STMT):
    return typecheck_WhileStmt(node);
    
  case NODETYPE_BIT(CASE_STMT):
    return typecheck_CaseStmt(node);

  case NODETYPE_BIT(READ_STMT):
    return typecheck_ReadStmt(node);

  case NODETYPE_BIT(WRITE_STMT):
    return typecheck_WriteStmt(node);

  default:
    break;
  }
  
  return 0;
}


/*==============================================================================
 * Plugging into the Abstract Syntax Tree
 *
 *============================================================================*/

/*
 * On the way through the tree, for the initial visit of each node:
 *  -Create a new scope if the node is a block node
 *  -For any constant and variable declarations, add them to the symbol table
 *    for the current scope
 *  -For all ambiguous identifiers, perform a lookup in the symbol table to
 *    determine its type in relation to its declaration.
 *  -While resolving ambiguous identifiers, a look up is performed to ensure
 *    an identifier was declared before it's use.
 */

static int preNodeVisit(int depth, TreeNode_t *node, void *data) {

  if (Analyze_GetStatus() != NONE)
    return -1;

  //skip null statements
  if (TreeNode_hasType(node, NULL_STMT))
    return 0;

  
  //if we hit a block, make a new scope
  if (TreeNode_hasType(node, BLOCK)) {
    enterScope(node);
  } else {
    
    //if inside a constant or variable section
    //through all children and their siblings to add them to the symbol table
    if (TreeNode_hasType(node, CONST_SEC) || TreeNode_hasType(node, VAR_SEC)) {
      TreeNode_t *section = TreeNode_getChild(node, 0);
      do {
        addSymbol(section);
        section = section->sibling;
      } while (section != NULL);
      
    }
    //not in const or variable section, check if node is an identifier of some sort
    //and resolve any ambiguities.
    else if (TreeNode_hasType(node, CONSTANT) || TreeNode_hasType(node, VARIABLE) ||
             TreeNode_hasType(node, SIMP_NAME)) {

      resolveVariables(node);
    }
  }

  return 0;
}



/*
 * On the way back up the tree:
 *  -Exit scopes to parent scope when leaving a block node
 *  -Resolve return types for variables and constants
 *  -Resolve return types for factors
 *  -Resolve return types for terms
 *  -Resolve return types for simple expressions, expressions, and conditions
 *  -With return types being resolved, type checking will occur for
 *    factors, terms, simple expressions, expressions, and conditions
 *  -Type checking for assignments
 *  -Type checking for read/write statements
 *  -Type checking for case statements
 */
static int postNodeVisit(int depth, TreeNode_t *node, void *data) {

  //we have hit an error, stop traversing the tree
  if (Analyze_GetStatus() != NONE)
    return -1;
  
  //don't stop tree traversing for null statments
  if (TreeNode_hasType(node, NULL_STMT))
    return 0;
  
  //if leaving a block, exit the scope
  if (TreeNode_hasType(node, BLOCK))
    exitScope();

  //type checking
  resolveReturnTypes(node);
  
  return 0;
}


/*==============================================================================
 * Public API
 *
 *============================================================================*/
SemanticErrors Analyze_GetStatus(void) {
  return errors;
}


int Analyze_Semantics(TreeNode_t *treeHead, bool verbose) {

  //initialize rodata table
  rodata = SymTable_init(SYMTAB_BASE_SIZE);
  if (!rodata) {
    fprintf(stderr, "Error initializing .rodata hash\n");
    return -1;
  }

  
  TreeNode_traverse(0, treeHead, NULL, preNodeVisit, postNodeVisit);

  //print out the fixed ast with symbol tables
  if (verbose && Analyze_GetStatus() == NONE) {
    fprintf(stdout, "\nSemantically Corrected AST\n");
    TreeNode_print(stdout, Parser_getTree(), true);
  }

  return 0;
}

SymTable_t *Analyze_GetRodata(void) {
  return rodata;
}


static int rodataCleanup(Symbol_t *entry, void *data) {
  
  if (entry->key)
    free(entry->key);

  memset(entry, 0, sizeof(Symbol_t));
  //entry freeing handled by SymTable_destroy
  return 0;
}

void Analyze_Cleanup(void) {
  SymTable_forEach(rodata, NULL, rodataCleanup);
  SymTable_destroy(rodata);
}

