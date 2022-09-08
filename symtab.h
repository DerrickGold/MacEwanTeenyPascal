/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Geneartion
 * Author: Derrick Gold
 *
 * Symbol Table API
 */
#ifndef __SYMBOL_TABLE_H__
#define __SYMBOL_TABLE_H__

#include <stdbool.h>
#include "lexer.h"


#define SYMTYPE_BITS_COUNT (sizeof(SymbolType) << 3)

#define SYMTYPE_BIT(x) (1 << (x))

#define SYMTYPE_INT(x) ( SYMTYPE_BIT(SYMTYPE_INT) | SYMTYPE_BIT((x)) )
#define SYMTYPE_STR(x) ( SYMTYPE_BIT(SYMTYPE_STR) | SYMTYPE_BIT((x)) )

/*
 * Symbol types are either:
 *  an Integer or String
 *
 * And Integers or Strings can be classified as a 
 * Constant or (Variable and/or Array)
 *
 * If the symbol is an array, it may have a 
 * Constant Reference type indicating that the
 * size of the array uses a constant identifier rather
 * than a numeric identifier.
 */
typedef enum {
  //type of data being held
  SYMTYPE_INT,
  SYMTYPE_STR,

  //data being held is referenced
  //by some identifier
  SYMTYPE_CONST_REF,
  
  //symbol classification
  SYMTYPE_CONSTANT,
  SYMTYPE_VARIABLE,
  SYMTYPE_ARRAY,
} SymbolType;

extern const char *SYM_TYPE_TEXT[];

//just re-use yystype
typedef yystype symdata;


typedef struct Symbol_s {
  char *key;
  symdata data;
  SymbolType type;
  int stackOffset;
} Symbol_t;


typedef struct SymTable_s {
  struct SymTable_s *parent;
  size_t count, size;
  Symbol_t **entries;
  int curStackPtr;
  int stackFrameDepth;
  int id;
} SymTable_t;


/*
 * Symbol_create:
 *  Create a symbol for entry in the symbol table.
 *
 * Arguments:
 *  key*: string to use for lookup.
 *  data*: string or integer data to associate with key
 *  type: Type of symbol that best represents the associated key/data
 *
 *  *: Only pointer copies of these fields are made, must exist
 *    for the life time of the table. That being said, these
 *    values are also not free'd by SymTable_destroy.
 *
 * Returns:
 *  Pointer to new populated symbol instance. NULL if symbol
 *  creation fails in any way.
 */
Symbol_t *Symbol_create(char *key, symdata data, SymbolType type);

/*
 * Symbol_addType:
 *  Add a type to a symbol. A symbol supports multiple
 *  types of the combinations specified below.
 *
 * Arguments:
 *  data: the symbol to modify
 *  type: SYMTYPE_INT, SYMTYPE_STR, SYMTYPE_CONSTANT,
 *    SYMTYPE_VARIABLE, SYMTYPE_ARRAY, or SYMTYPE_CONST_REF
 *
 * Type Combinations:
 *    Either Integer or String:  
 *      SYMTYPE_INT
 *      SYMTYPE_STR
 *
 *    And or Constant, Variable, or Integer 
 *      SYMTYPE_CONSTANT
 *      SYMTYPE_VARIABLE
 *      SYMTYPE_ARRAY
 *
 *    And if type array that references a constant for size:
 *      SYMTYPE_CONST_REF
 *    
 */
void Symbol_addType(Symbol_t *data, SymbolType type);

/*
 * Symbol_rmType:
 *  Remove a type from a symbol.
 *
 * Arguments:
 *  data: The symbol to modify
 *  type: SYMTYPE_INT, SYMTYPE_STR, SYMTYPE_CONSTANT,
 *    SYMTYPE_VARIABLE, SYMTYPE_ARRAY, or SYMTYPE_CONST_REF
 */
void Symbol_rmType(Symbol_t *data, SymbolType type);

/*
 * Symbol_hasType:
 *  Check if a symbol is of a type.
 *
 * Arguments:
 *  data: The symbol to check
 *  type: SYMTYPE_INT, SYMTYPE_STR, SYMTYPE_CONSTANT,
 *    SYMTYPE_VARIABLE, SYMTYPE_ARRAY, or SYMTYPE_CONST_REF
 *
 * Returns:
 *  True if symbol has the specified type, False otherwise.
 */
bool Symbol_hasType(Symbol_t *data, SymbolType type);


Symbol_t *Symbol_getArraySizeEntry(SymTable_t *table, Symbol_t *array);

/*
 * Symbol_print:
 *  Print output a symbol table entry
 *
 * Arguments:
 *  output: File stream to write to
 *  entry: symbol to write output of.
 *
 */
void Symbol_print(FILE *output, Symbol_t *entry);

/*
 * SymTable_init
 *  Initialize a symbol table instance.
 *
 * Arguments:
 *  size: initial number of elements to allocate in symbol table.
 *
 * Returns:
 *  A pointer to a Symbol table instance. NULL if any error occured
 *  in initialization.
 */
SymTable_t *SymTable_init(size_t size);

/*
 * SymTable_destroy:
 *  Free all memory allocated by a symbol table instance.
 *  Does not free key pointers nor pointers used in symbol
 *  data.
 *
 * Arguments:
 *  table: Symbol table to destroy
 *
 */
void SymTable_destroy(SymTable_t *table);

/*
 * Symbol_getEntry:
 *  Get a pointer to the symbols position in the symbol table.
 *
 * Arguments:
 *  table: Symbol table instance to search in
 *  key: Key to look up symbol with
 *  
 * Returns:
 *  A pointer to a location in the symbol table where the
 *  symbol should reside. Does not return the symbol itself.
 *  To get the symbol from this location, one just needs to
 *  dereference the return value, or use SymTable_find instead.
 */
Symbol_t **SymTable_getEntry(SymTable_t *table, char *key);

/*
 * SymTable_add:
 *  Add symbol to symbol table.
 *
 * Arguments:
 *  table: Symbol table to add symbol into
 *  data: symbol to add into symbol table.
 *
 * Returns:
 *  Location in Symbol Table in which the symbol was added to.
 *  NULL if no table or symbol arguments given.
 */
Symbol_t **SymTable_add(SymTable_t *table, Symbol_t *data);


/*
 * SymTable_find:
 *  A wrapper for SymTable_getEntry. Retreive a symbol 
 *  from a symbol table.
 *
 * Arguments:
 *  table: Symbol table to search for symbol in
 *  key: Key to look up symbol with
 *
 * Returns:
 *  A pointer to a symbol instance.
 */
Symbol_t *SymTable_find(SymTable_t *table, char *key);

Symbol_t *SymTable_findAll(SymTable_t *table, char *key, int *bytesOff);

/*
 * SymTable_print:
 *  Print out a symbol table.
 *
 * Arguments:
 *  output: file stream to write table output to
 *  talbe: the table to print out.
 */
void SymTable_print(FILE *output, SymTable_t *table);


void SymTable_addStackVar(SymTable_t *table, Symbol_t *symbol);

int SymTable_getStackDepth(SymTable_t *table);

int SymTable_forEach(SymTable_t *table, void *data, int (*fn) (Symbol_t *, void *));

int SymTable_addParent(SymTable_t *table, SymTable_t *parent);
#endif

