/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Symbol Table API
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "symtab.h"
#include "defines.h"

//size of variable on the stack
#define VAR_STACK_SIZE WORD_SIZE_BYTES

//Limit how many times to rehash an entry before
//resizing the hash table
#define LOOKUP_ATTEMPTS 5

//how large to grow the table each time it is
//automatically resized. As a percentage of its
//current size.
#define GROWTH_RATE 0.21f


//fancy symbol table printing macros
#define ID_COL_SIZE 15
#define TYPES_COL_SIZE 21
#define STACK_COL_SIZE 14
#define VALUE_COL_SIZE 32

#define PRINT_ROW_FMT "%-*s|%-*s|%-*s|%-*s\n"
#define PRINT_HEADER "Symbol Table:============================================================================:\n"
#define PRINT_COLUMNS PRINT_ROW_FMT, ID_COL_SIZE,"Identifiers",STACK_COL_SIZE,"Stack Offset",TYPES_COL_SIZE,"Type",\
    VALUE_COL_SIZE,"Value"

#define PRINT_FOOTER "=======================================================================Depth %-4d:Size %-4d\n"
#define PRINT_DIVIDER "-----------------------------------------------------------------------------------------\n"

#define MORE_FOLLOWING "..."
#define NO_VAL ""
#define NO_STACK "NONE"

#define PRINT_ROW(id,stack,type,val) \
  PRINT_ROW_FMT,ID_COL_SIZE,id,STACK_COL_SIZE,stack,TYPES_COL_SIZE,type,VALUE_COL_SIZE,val



const char *SYM_TYPE_TEXT[] = {
  "Integer",
  "String",
  "Ref",
  "Constant",
  "Variable",
  "Array",
};



static size_t growSize(size_t oldSize) {
  
  return oldSize  + (size_t)floor(oldSize * GROWTH_RATE);
}


Symbol_t *Symbol_create(char *key, symdata data,  SymbolType type) {

  Symbol_t *entry = calloc(1, sizeof(Symbol_t));
  if (!entry) {
    fprintf(stderr, "Error allocating hash data\n");
    return NULL;
  }
  
  //set the key to whatever the token was pointing at
  entry->key = key;
  entry->type = type;

  entry->data = data;
  return entry;
}

void Symbol_destroy(Symbol_t *data) {

  if (!data)
    return;

  memset(data, 0, sizeof(Symbol_t));
  free(data);
}

static int symbolDestroyHelper(Symbol_t *entry, void *data) {
  Symbol_destroy(entry);
  return 0;
}


void Symbol_addType(Symbol_t *data, SymbolType type) {

  if (!data)
    return;

  data->type |= SYMTYPE_BIT(type);
}

void Symbol_rmType(Symbol_t *data, SymbolType type) {

  if (!data)
    return;

  data->type &= ~SYMTYPE_BIT(type);
}

bool Symbol_hasType(Symbol_t *data, SymbolType type) {

  if (!data)
    return false;

  return data->type & SYMTYPE_BIT(type);
}




/*
 * Helper function for looking up the size of an array.
 * If the size value of an array references a constant,
 * will look up that constant value and return that
 * symbol entry.
 */
Symbol_t *Symbol_getArraySizeEntry(SymTable_t *table, Symbol_t *array) {

  Symbol_t *curEntry = array;

  if (Symbol_hasType(curEntry, SYMTYPE_CONST_REF))
    curEntry = SymTable_findAll(table, curEntry->data.string, NULL);

  
  return curEntry;
}

void Symbol_print(FILE *output, Symbol_t *entry) {

  if (!entry || !entry->key)
    return;
  
  char identifierBuf[ID_COL_SIZE];
  memset(identifierBuf, 0, ID_COL_SIZE);
  char typesBuf[TYPES_COL_SIZE];
  memset(typesBuf, 0, TYPES_COL_SIZE);
    
  if (strlen(entry->key) > ID_COL_SIZE) {
    strncpy(identifierBuf, entry->key, ID_COL_SIZE - 1  - strlen(MORE_FOLLOWING));
    //strcat(identifierBuf, MORE_FOLLOWING);
    SAFECAT(identifierBuf, MORE_FOLLOWING, ID_COL_SIZE);
  } else
    strncpy(identifierBuf, entry->key, ID_COL_SIZE);

  SymbolType oldType = entry->type;
  
  //print symbol types
  char symbolShort[4];
  memset(symbolShort, 0, sizeof(symbolShort));
  for (int bit = 0; bit < SYMTYPE_BITS_COUNT && oldType > 0; bit++) {
    if (!Symbol_hasType(entry, bit))
      continue;
    
    //unset the type
    oldType &= ~SYMTYPE_BIT(bit);

    //only print initials of type text
    strncpy(symbolShort, SYM_TYPE_TEXT[bit], sizeof(symbolShort) - 1);
    SAFECAT(typesBuf, symbolShort, TYPES_COL_SIZE);
    //check if we are on the last set type
    if (oldType)
      SAFECAT(typesBuf, ", ", TYPES_COL_SIZE);
    
  }
  
  //print out the entry
  
  if (Symbol_hasType(entry, SYMTYPE_CONSTANT) || Symbol_hasType(entry, SYMTYPE_ARRAY)) {
    
    char stackNum[NUM_TO_STR_BUF];
    if (Symbol_hasType(entry, SYMTYPE_CONSTANT))
      snprintf(stackNum, NUM_TO_STR_BUF, NO_STACK);
    else
      snprintf(stackNum, NUM_TO_STR_BUF, "%d", entry->stackOffset);

    
    if (Symbol_hasType(entry, SYMTYPE_STR) || Symbol_hasType(entry, SYMTYPE_CONST_REF)) {
      //trim longer values to fit VALUE_COL_SIZE
      char outvalue[VALUE_COL_SIZE];
      memset(outvalue, 0, VALUE_COL_SIZE);
      strncpy(outvalue, entry->data.string, VALUE_COL_SIZE - 1  - strlen(MORE_FOLLOWING));
      SAFECAT(outvalue, MORE_FOLLOWING, VALUE_COL_SIZE - 1);
      fprintf(output, PRINT_ROW(identifierBuf, stackNum, typesBuf, outvalue));
    } else {
      char valueNum[NUM_TO_STR_BUF];
      snprintf(valueNum, sizeof(valueNum), "%d", entry->data.value);
      fprintf(output, PRINT_ROW(identifierBuf, stackNum, typesBuf, valueNum));
    }
  }
  else {
    char stackNum[NUM_TO_STR_BUF];
    snprintf(stackNum, NUM_TO_STR_BUF, "%d", entry->stackOffset);
    fprintf(output, PRINT_ROW(identifierBuf, stackNum, typesBuf, NO_VAL));
  }
}

static int symbolPrintHelper(Symbol_t *symbol, void *data) {
  
  Symbol_print((FILE *)data, symbol);
  return 0;
}

//djb2 algorithm
//http://www.cse.yorku.ca/~oz/hash.html
size_t hash1(char *key, size_t num, size_t last) {

  size_t hashVal = 5381 + last;
  int c;

  while ((c = *key++) != '\0')
    hashVal = ((hashVal << 5) + hashVal) ^ c; /* hash * 33 + c */


  return hashVal % num;
}

size_t hash2(char *key, size_t num, int attempt1) {

  size_t hashVal = 0;
  while (*key != '\0') {
    hashVal = ((hashVal << 4) + *key) % num;
    key++;
  }

  return (attempt1 - hashVal) % num;
}



Symbol_t **SymTable_getEntry(SymTable_t *table, char *key) {

  if (!table || !key)
    return NULL;

  size_t pos = hash1(key, table->size, 0);

  Symbol_t **curPos = &table->entries[pos];

  //no table entries allocated?
  if (!curPos)
    return NULL;
    
  //if first attempt and nothing exists, return the free position
  if (!*curPos)
    return curPos;

  //otherwise, check that we've got the right one
  int attempt = 0;
  
  do {
    Symbol_t *entry = *curPos;
    
    if (!strcmp(key, entry->key))
      return curPos;
    
    //pos = (pos + attempt) % table->size;
    pos = hash1(key, table->size, pos + (attempt<<1));
    curPos = &table->entries[pos];
  } while (*curPos != NULL && attempt++ < LOOKUP_ATTEMPTS);

  if (attempt >= LOOKUP_ATTEMPTS)
    return NULL;

  return curPos;
}



int SymTable_copy(SymTable_t *dest, SymTable_t *src) {

  //make sure the dest table is at least as large as the source
  if (dest->size < src->size)
    return -1;
  
  //rehash all entries and copy them over
  for (size_t e = 0; e < src->size; e++) {

    //skip null entries
    if (src->entries[e] == NULL)
      continue;

    //have a valid entry
    Symbol_t **pos = SymTable_getEntry(dest, src->entries[e]->key);
    if (!pos) {
      return -1;
    }

    if (*pos == NULL)
      *pos = src->entries[e];
    else {
      return -1;
    }
    
  }

  dest->curStackPtr = src->curStackPtr;
  dest->stackFrameDepth = src->stackFrameDepth;
  dest->id = src->id;
  return 0;
}

SymTable_t *SymTable_init(size_t size) {

  static int nextID = 0;
  
  if (size <= 0)
    return NULL;

  SymTable_t *table = calloc(1, sizeof(SymTable_t));
  if (!table) {
    fprintf(stderr, "SymTable_init: Error allocating symbol table\n");
    return NULL;
  }
  table->size = size;
  table->entries = calloc(size, sizeof(Symbol_t*));
  if (!table->entries) {
    fprintf(stderr, "SymTable_init: Error allocating symtable entries\n");
    free(table);
    return NULL;
  }

  table->id = nextID++;

  return table;
}



void SymTable_destroy(SymTable_t *table) {

  if (!table)
    return;


  if (table->entries) {    
    SymTable_forEach(table, NULL, symbolDestroyHelper);
    free(table->entries);
  }

  memset(table, 0, sizeof(SymTable_t));
  free(table);
}


int SymTable_resize(SymTable_t *table, size_t size) {

  SymTable_t *newTable = NULL;
  int status = 0;
  do {
    newTable = SymTable_init(size);
    if (!newTable) {
      fprintf(stderr, "Error initializing new table\n");
      return -1;
    }

    status = SymTable_copy(newTable, table);

    //error copying data, resize the table 
    if (status) {
      size = growSize(size);
      free(newTable);
    }
  } while (status);
    
  //then free old table, and assign new table
  free(table->entries);

  //make original table point to new table stuff
  table->entries = newTable->entries;
  table->size = newTable->size;
  free(newTable);
  //and done
  return 0;
}



void SymTable_print(FILE *output, SymTable_t *table) {

  if (!table || !table->entries)
    return;

  fprintf(output, PRINT_HEADER);
  fprintf(output, PRINT_COLUMNS);
  fprintf(output, PRINT_DIVIDER);
  SymTable_forEach(table, (void *)output, symbolPrintHelper);
  fprintf(output, PRINT_FOOTER, table->stackFrameDepth, table->curStackPtr);
}


int SymTable_forEach(SymTable_t *table, void *data, int (*fn) (Symbol_t *, void *)) {
  
  if (!table || !fn)
    return 0;
  
  //loop through all symbol table entries
  for (size_t i = 0; i < table->size; i++) {

    //skip entries where key may not be set (malformed entries)
    Symbol_t *entry = table->entries[i];
    
    //skip empty entries
    if (!entry)
      continue;

    int status = fn(entry, data);
    if (status)
      return status;

  } //for each table element

  return 0;
}

Symbol_t **SymTable_add(SymTable_t *table, Symbol_t *data) {

  if (!table || !table->entries)
   return NULL;

  //if we somehow managed to fill the table, make it grow
  if (table->count >= table->size)
    SymTable_resize(table, growSize(table->size));

       
  Symbol_t **position = SymTable_getEntry(table, data->key);
  
  if (!position) {
    SymTable_resize(table, growSize(table->size));
    //try adding the data again
    return SymTable_add(table, data);;

  }
  if (!*position) {
    (*position) = data;
    table->count++;
  }
  //if this statement is false, that means an entry already
  //exists for a given key, just return that data instead
  return position;
}

Symbol_t *SymTable_find(SymTable_t *table, char *key) {
    
  Symbol_t **position = SymTable_getEntry(table, key);
  if (!*position) {
    return NULL;
  }

  return *position;
}

Symbol_t *SymTable_findAll(SymTable_t *table, char *key, int *bytesOff) {

  SymTable_t *scope = table;
  Symbol_t *found = NULL;

  int bytes = 0;
  //try initial look up
  found = SymTable_find(scope, key);
  if (found) {
    if (bytesOff)
      *bytesOff = bytes;

    return found;
  }

  scope = scope->parent;
  //then go through parents
  while (!found && scope != NULL) {
    found = SymTable_find(scope, key);
    bytes += scope->curStackPtr;
    bytes += 4; //add stack frame
    scope = scope->parent;
  }

  //get the beginning of the current stack
  if (bytesOff)
    *bytesOff = bytes;
    
  return found;
}


void SymTable_addStackVar(SymTable_t *table, Symbol_t *symbol) {
  
  //store stack offset for variable
  symbol->stackOffset = table->curStackPtr;
  //then increase the stack pointer for the current scope
  if (Symbol_hasType(symbol, SYMTYPE_ARRAY)) {
    Symbol_t *size = Symbol_getArraySizeEntry(table, symbol);
    table->curStackPtr += VAR_STACK_SIZE * size->data.value;
  } else
    table->curStackPtr += VAR_STACK_SIZE;
}

int SymTable_getStackDepth(SymTable_t *table) {

  return table->stackFrameDepth;
}

int SymTable_addParent(SymTable_t *table, SymTable_t *parent) {

  if (!table || !parent)
    return -1;
  
  table->parent = parent;

  //update locations
  int depth =SymTable_getStackDepth(parent);
  table->stackFrameDepth = depth + 1;
  return 0;
}
