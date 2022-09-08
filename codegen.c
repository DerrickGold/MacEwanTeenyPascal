/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Code Generation from an abstract syntax tree, produces x86
 * assembly code.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "tree.h"
#include "symtab.h"
#include "parser.h"
#include "defines.h"
#include "bittree.h"

#define COMMENT_BUF_LEN 256

//Maximum number of elements to use a lookup
//table for with case statements. Cases with
//numbers greater than this value will use
//an alternative method
#define MAX_LOOKUP_SIZE 256

#define WRITE_LABEL_WIDTH 20
//longest instruction is 7 characters long plus 1 space after it
#define WRITE_INST_WIDTH 8 
//make width enough for at least 2 labels, a comma, and end space
#define WRITE_ARGS_WIDTH ((WRITE_LABEL_WIDTH * 2) + 2)


//filter for statements
#define STMT_FILTER ( \
  NODETYPE_BIT(IF_STMT) | NODETYPE_BIT(READ_STMT) | NODETYPE_BIT(WRITE_STMT) | \
  NODETYPE_BIT(CASE_STMT) | NODETYPE_BIT(ASSIGN_STMT) | NODETYPE_BIT(BLOCK_STMT) | \
  NODETYPE_BIT(WHILE_STMT) | NODETYPE_BIT(STMT_LIST)                    \
)

#define EXP_FILTER ( \
  NODETYPE_BIT(RELOP) | NODETYPE_BIT(BINOP) | NODETYPE_BIT(UNARYOP) | \
  NODETYPE_BIT(MULOP) | NODETYPE_BIT(VARIABLE) | NODETYPE_BIT(CONSTANT) \
)

#define FILE_HEADER (                                           \
";=======================================================\n"    \
"; Code generated from the MacEwan Teeny Pascal Language\n"     \
"; Compiler written by Derrick Gold\n"                          \
"; CMPT 399 (Winter 2016)\n"                                    \
";=======================================================\n"    \
)


//number of values to fit per line in a multi-line
//variable declaration
#define DECLS_PER_LINE 3

#define LABEL_TEXT "LABEL"
#define LABEL_FMT (LABEL_TEXT"%d")

#define TABLE_TEXT "CASE"
#define TABLE_FMT (TABLE_TEXT"%d")
#define TABLE_TAG_FMT ("_%d")
#define TABLE_TAG_DEF ("_default")
#define TABLE_TAG_END ("_end")

#define MAIN_LABEL "main"


//current stack frame
#define REG_STACKFRAME "ebp"
//current stack addr
#define REG_STACKPTR "esp"
//operation results always stored here
#define REG_RETURN "eax"
#define REG_RETURN_SHORT "ah"
#define REG_RETURN_BYTE "al"
//variable addresses are stored here when they are read
#define REG_VARADDR "ecx"
//free register to do whatever with
#define REG_FREE "ebx"
#define REG_FREE_BYTE "bl"
//register for storing shift values
#define REG_SHIFT "cl"

#define DEREF_REG(reg) ("["reg"]")

//referencing local variable in a scope
#define STACK_VAR_FMT "["REG_STACKFRAME"%+d]"

//push value on the stack
#define STORE_RESULT(reg) do {                     \
  writeLine(output, true, NULL, "push", NULL, 1, reg); \
} while (0)

//pop value off the stack
#define RESTORE_RESULT(reg) do {                  \
  writeLine(output, true, NULL, "pop", NULL, 1, reg);  \
} while (0)

//write out a blank line
#define BLANK_LINE do {                         \
  writeLine(output, true, NULL, NULL, NULL, 0);  \
} while (0)

//write out a comment on its own line
#define COMMENT_LINE(comment) do {             \
  BLANK_LINE;                                \
  writeLine(output, true, NULL, NULL, comment, 0); \
} while (0)


//cleanup after a read/write call
#define CLEANUP_CALLSTACK(argc) do {                                    \
  char numBuf[NUM_TO_STR_BUF];                                        \
  int bytes = (argc) * WORD_SIZE_BYTES;                               \
  snprintf(numBuf, NUM_TO_STR_BUF, "%d", bytes);                        \
  writeLine(output, true, NULL, "add", makeComment(CLEAN_CALL, bytes), 2, REG_STACKPTR, numBuf); \
} while (0)


//set a register to 0
#define CLEAR_REGISTER(reg) do {                      \
  writeLine(output, true, NULL, "mov", NULL, 2, (reg), "0"); \
} while (0)

//compare a register to 0
#define TEST_REGISTER(reg) do {                      \
  writeLine(output, true, NULL, "cmp", NULL, 2, (reg), "0"); \
} while (0)

#define ASM_LINE(cmd, argc, ...) do {                                 \
  if (argc > 0)                                                     \
    writeLine(output, true, NULL, cmd, NULL,  (argc), ##__VA_ARGS__);   \
  else                                                                \
    writeLine(output, true, NULL, cmd, NULL, 0);                      \
} while (0)

#define SECTION_LINE(section) do {                            \
  writeLine(output, true, NULL, "section", NULL, 1, section); \
 } while (0)



#define MAKE_LABEL(output, outputLen)           \
  makeLabel(LABEL_FMT, output, (outputLen))


#define CASE_TAGGED_LABEL(output, outputLen, tag, value) do { \
  makeLabelEx(TABLE_FMT, output, (outputLen), tableStart);  \
  tagLabel(output, outputLen, tag, (value));                \
} while (0)

#define MULTILINE_DECL(label, count, perLine) do { \
  if ((count) == 0)                                 \
    writeLine(output, false, NULL, "dd", NULL, 1, label);   \
  else {                                                    \
    fprintf(output, ",");                                   \
    if ((count) % (perLine) == 0) {                         \
      fprintf(output, "\n");                                \
      writeLine(output, false, NULL, "dd", NULL, 0);        \
    }                                                       \
    fprintf(output, "%s", label);                           \
  }                                                         \
} while (0)


#define SHORT_RECURSION(label, node, tokenType, fn) do {    \
    if (node->token->type == (tokenType)) {       \
      if (fn(output, node, shortLabel)) {       \
  if (label) free(label);           \
  return -1;              \
      }                 \
    } else {                \
      if (generateExp(output, node)) {          \
  if (label) free(label);           \
  return -1;              \
      }                 \
    }                 \
  } while (0)

#define SHORTCIRCUIT_CONDITION(label, tokenType, fn) do {   \
    if (!shortLabel) {              \
      label = calloc(1, COMMENT_BUF_LEN);       \
      if (!label) {             \
  fprintf(stderr, "Error allocating short circuit label\n");  \
  return -1;              \
      }                 \
      MAKE_LABEL(label, COMMENT_BUF_LEN);       \
      shortLabel = label;           \
    }                 \
                      \
    TreeNode_t *left = TreeNode_getChild(node, 0),      \
      *right = TreeNode_getChild(node, 1);        \
                      \
    SHORT_RECURSION(label, left, tokenType, fn);        \
                      \
    STORE_RESULT(REG_RETURN);           \
                  \
    SHORT_RECURSION(label, right, tokenType, fn);     \
  } while (0)


#define EXIT_PRGM do { \
    writeLine(output, true, "exit", "mov", NULL, 2, REG_RETURN, "0"); \
    writeLine(output, true, NULL, "ret", NULL, 0);                     \
} while (0) 


typedef enum {
  NEW_SCOPE,
  LEAVE_SCOPE,
  ARRAY_INDEX,
  LOAD_VAR,
  CLEAN_CALL,
  UNARY_MINUS,
  ASSIGN_TO,
  LOG_NEG,
  SHORT_CIRCUIT_OR,
  SHORT_CIRCUIT_AND,
  SHORTED_OR,
  SHORTED_AND,
  NO_OPERATOR,
  CASE_LABEL,
} GENERATE_COMMENT;

const char *COMMENT_STRINGS[] = {
  "Enter new scope %d.",
  "Leaving scope %d.",
  "Indexing Array: '%s'",
  "Loading Variable: '%s'",
  "Cleanup call stack",
  "Unary Negation",
  "Assignment to '%s'",
  "!%s",
  "Short Circuit Or",
  "Short Circuit And",
  "Shorted Or",
  "Shorted And",
  "Invalid operator was found: token type: %d",
  "Case State: %d",
};

int generateStatement(FILE *output, TreeNode_t *node);
int generateExp(FILE *output, TreeNode_t *node);


static SymTable_t *currentScope = NULL;

static void writeLine(FILE *output, bool newline, char *label, char *instruction, char *comment, int argc, ...) {
  
  char commentLine = (!label && !instruction); 
  
  //print out label
  if (label)
    fprintf(output, "%-*s: ", WRITE_LABEL_WIDTH, label);
  else
    fprintf(output, "%-*s  ", WRITE_LABEL_WIDTH, EMPTY_STR);

  
  if (instruction) {
    fprintf(output, "%-*s", WRITE_INST_WIDTH, instruction);

    //print arguments
    if (argc > 0) {
      char argsBuf[WRITE_ARGS_WIDTH  + 1];
      char *pos = argsBuf;
      memset(argsBuf, 0, sizeof(argsBuf));
      
      va_list args;
      va_start(args, argc);
      
      for (int i = 0; i < argc - 1; i++)
        pos += sprintf(pos, "%s, ", va_arg(args, char *));
      sprintf(pos, "%s", va_arg(args, char *));
      
      va_end(args);
      fprintf(output, "%-*s", WRITE_ARGS_WIDTH, argsBuf);
    }
  }
  else if (!commentLine) {
    //instruction padding
    fprintf(output, "%*s", WRITE_INST_WIDTH, EMPTY_STR);
    //args padding
    fprintf(output, "%*s", WRITE_ARGS_WIDTH, EMPTY_STR);
  }
  
  //print comment
  if (comment)
    fprintf(output, "; %s", comment);

  if (newline)
    fprintf(output, "\n");
}



static char *makeComment(GENERATE_COMMENT msg, ...) {
  static char commentbuf[COMMENT_BUF_LEN];

  va_list values;
  va_start(values, msg);
  
  vsnprintf(commentbuf, COMMENT_BUF_LEN, COMMENT_STRINGS[msg], values);
  va_end(values);
  
  return commentbuf;
}


//generate a new label for use in the
//assembly output
static void makeLabelEx(const char *prefix, char *output, size_t outputLen, int num) {
  
  if (!prefix || !output)
    return;

  size_t len = outputLen > WRITE_LABEL_WIDTH ? WRITE_LABEL_WIDTH : outputLen;
  snprintf(output, len, prefix, num);
}

static int makeLabel(const char *prefix, char *output, size_t outputLen) {
  
  static int labelCount = 0;   
  makeLabelEx(prefix, output, outputLen, labelCount);
  return labelCount++;
}

static void tagLabel(char *output, size_t outputLen, const char *tagFmt, int value) {
  
  char tagBuffer[COMMENT_BUF_LEN];
  snprintf(tagBuffer, COMMENT_BUF_LEN, tagFmt, value);
  SAFECAT(output, tagBuffer, outputLen);
}


static int getConstInteger(TreeNode_t *node) {
  int value = -1;
  if (node->entry)
    value = node->entry->data.value;
  else
    value = node->token->lexeme.value;

  return value;
}


/*
 * in a block, need to setup the stack, allocate memory for 
 * local variables on the stack
 * Note: check enter and leave asm instructions
 */
static int generateBlockStmt(FILE *output, TreeNode_t *node) {

  char stackOffsetbuf[NUM_TO_STR_BUF];
  snprintf(stackOffsetbuf, NUM_TO_STR_BUF, "%d", node->symbols->curStackPtr);
  
  COMMENT_LINE(makeComment(NEW_SCOPE, node->symbols->stackFrameDepth));

  STORE_RESULT(REG_STACKFRAME);
  ASM_LINE("mov", 2, REG_STACKFRAME, REG_STACKPTR);
  ASM_LINE("sub", 2, REG_STACKPTR, stackOffsetbuf);
  
  //keep track of what scope we're at
  currentScope = node->symbols;
  //explore further statements
  int status = generateStatement(output, TreeNode_getChild(node, 2));
  
  //restore stack
  char *endScopeComment = makeComment(LEAVE_SCOPE, node->symbols->stackFrameDepth);
  writeLine(output, true, NULL, "mov", endScopeComment, 2, REG_STACKPTR, REG_STACKFRAME);
  RESTORE_RESULT(REG_STACKFRAME);
  
  //exit current scope
  currentScope = node->symbols->parent;
  
  return status;
}

static int generateIfStmt(FILE *output, TreeNode_t *node) {

  COMMENT_LINE("If Statement...");
  TreeNode_t *condition = TreeNode_getChild(node, 0),
    *trueCase = TreeNode_getChild(node, 1),
    *elseCase = TreeNode_getChild(node, 2);

  //evaluate the condition first
  if (generateExp(output, condition))
    return -1;

  COMMENT_LINE("Condition evaluated");
  char falseLabel[COMMENT_BUF_LEN],
    endLabel[COMMENT_BUF_LEN];
  
  MAKE_LABEL(falseLabel, COMMENT_BUF_LEN);
  MAKE_LABEL(endLabel, COMMENT_BUF_LEN);

  //test for false case
  TEST_REGISTER(REG_RETURN);
  ASM_LINE("je", 1, falseLabel);

  if (generateStatement(output, trueCase))
    return -1;

  //skip else case in true case
  ASM_LINE("jmp", 1, endLabel);
  
  //write out false case now
  writeLine(output, true, falseLabel, NULL, NULL, 0);

  if (elseCase && generateStatement(output, elseCase))
    return -1;
  
  writeLine(output, true, endLabel, NULL, NULL, 0);
  return 0;
}

static int generateWhileStmt(FILE *output, TreeNode_t *node) {

  TreeNode_t *condition = TreeNode_getChild(node, 0),
    *loopCase = TreeNode_getChild(node, 1);

  char repeatLabel[COMMENT_BUF_LEN],
    exitLabel[COMMENT_BUF_LEN];

  MAKE_LABEL(repeatLabel, COMMENT_BUF_LEN);
  MAKE_LABEL(exitLabel, COMMENT_BUF_LEN);

  writeLine(output, true, repeatLabel, NULL, "While loop", 0);
  //evaluate the condition first
  if (generateExp(output, condition))
    return -1;

  COMMENT_LINE("Condition evaluated");
  
  //test for false case
  TEST_REGISTER(REG_RETURN);
  ASM_LINE("je", 1, exitLabel);

  //evaluate loop contents
  if (generateStatement(output, loopCase))
    return -1;

  //jump back to process loop condition again
  ASM_LINE("jmp", 1, repeatLabel);
  
  //write out the exit label
  writeLine(output, true, exitLabel, NULL, "Exit While", 0);
  return 0;
}

//assign to variables
static int generateAssignStmt(FILE *output, TreeNode_t *node) {

  TreeNode_t *left = TreeNode_getChild(node, 0),
    *right = TreeNode_getChild(node, 1);

  //evaluate l-value
  if (generateExp(output, left))
    return -1;

  //store memory address of variable/indexed array we are assigning to
  STORE_RESULT(REG_VARADDR);
  
  //evaluate r-value
  if (generateExp(output, right))
    return -1;

  //r-value in REG_RETURN, with address in REG_VARADDR if variable (or string constant)
  //load l-val (dest address) into free register
  RESTORE_RESULT(REG_FREE);

  //move values from src to dest
  writeLine(output, true, NULL, "mov", makeComment(ASSIGN_TO, left->entry->key), 2, DEREF_REG(REG_FREE), REG_RETURN);
  return 0;
}

int generateWriteStmt(FILE *output, TreeNode_t *node) {

  COMMENT_LINE("Write Call...");
  TreeNode_t *args[node->argc];
  
  TreeNode_t *firstArg = TreeNode_getChild(node, 0);
  
  //flip argument order
  for (int i = node->argc - 1; i >= 0; i--) {
    args[i] = firstArg;
    firstArg = firstArg->sibling;
  }

  //now go through arguments in proper order
  for (int i = 0; i < node->argc; i++) {
    TreeNode_t *curArg = args[i];
    if (generateExp(output, curArg))
      return -1;

    //write the value
    if (TreeNode_hasType(curArg, VARIABLE))
      ASM_LINE("push", 1, REG_RETURN);
    else
      ASM_LINE("push", 1, "DWORD "REG_RETURN);

    
    //write what type to print value as
    switch (TreeNode_getReturnType(curArg)) {
    case RETURN_INT:
      ASM_LINE("push", 1, "DWORD 0");
      break;
    case RETURN_STR:
      ASM_LINE("push", 1, "DWORD 1");
      break;
    case RETURN_BOOL:
      ASM_LINE("push", 1, "DWORD 2");
      break;

    default:
      break;
    }
  }
  //write number of arguments now
  size_t countBufLen = NUM_TO_STR_BUF + strlen("DWORD ") + 1;
  char countBuf[countBufLen];
  snprintf(countBuf, countBufLen, "DWORD %d", node->argc);
  //push argc value for write function
  ASM_LINE("push", 1, countBuf);
  
  //call the function
  ASM_LINE("call", 1, "write");
  
  //fix stack on function exit, multiply by 2 since we are pushing
  //2 values per arg, + 1 for the number of arguments
  CLEANUP_CALLSTACK((node->argc * 2) + 1);
  return 0;
}

static int generateReadStmt(FILE *output, TreeNode_t *node) {
  
  COMMENT_LINE("Write Call");
  TreeNode_t *args[node->argc];
  
  TreeNode_t *firstArg = TreeNode_getChild(node, 0);
  
  //flip argument order
  for (int i = node->argc - 1; i >= 0; i--) {
    args[i] = firstArg;
    firstArg = firstArg->sibling;
  }
  
  //now go through arguments in proper order
  for (int i = 0; i < node->argc; i++) {
    TreeNode_t *curArg = args[i];
    if (generateExp(output, curArg))
      return -1;

    //should have generated a variable in ecx
    ASM_LINE("push", 1, REG_VARADDR);
  }

  //push number of arguments
  //write number of arguments now
  size_t countBufLen = NUM_TO_STR_BUF + strlen("DWORD ") + 1;
  char countBuf[countBufLen];
  snprintf(countBuf, countBufLen, "DWORD %d", node->argc);
  //push argc value for write function
  ASM_LINE("push", 1, countBuf);
  ASM_LINE("call", 1, "read");
  CLEANUP_CALLSTACK(node->argc + 1);
  return 0;
}


static int generateCaseStmt(FILE *output, TreeNode_t *node) {

  TreeNode_t *condition = TreeNode_getChild(node, 0),
    *cases = TreeNode_getChild(node, 1),
    *defaultCase = TreeNode_getChild(node, 2);

  //make some important labels (lookup table start, default label, and end of switch)
  char tableLabel[COMMENT_BUF_LEN],
    tempLabel[COMMENT_BUF_LEN],
    defaultLabel[COMMENT_BUF_LEN],
    endCase[COMMENT_BUF_LEN];
  
  int tableStart = makeLabel(TABLE_FMT, tableLabel, COMMENT_BUF_LEN);
  
  CASE_TAGGED_LABEL(defaultLabel, COMMENT_BUF_LEN, TABLE_TAG_DEF, 0);
  CASE_TAGGED_LABEL(endCase, COMMENT_BUF_LEN, TABLE_TAG_END, 0);

  char maxCaseStr[NUM_TO_STR_BUF];
  int maxCaseVal = node->argc;
  snprintf(maxCaseStr, NUM_TO_STR_BUF, "%d", maxCaseVal);
  
  COMMENT_LINE("Switch Start");
  //evaluate condition
  if (generateExp(output, condition))
    return -1;


  TreeNode_t *curCase = cases;

  //if the number of potential cases is small enough
  //implement a lookup table
  if (maxCaseVal <= MAX_LOOKUP_SIZE) {
    COMMENT_LINE("Switch Lookup");
    //store the result for later
    
    //check if the expression evaluated to a value greater than the max case state
    //if so, jump right to default case
    ASM_LINE("cmp", 2, REG_RETURN, maxCaseStr);
    ASM_LINE("ja", 1, defaultLabel);
    
    //otherwise, perform lookup
    ASM_LINE("mov", 2, REG_FREE, tableLabel);
    ASM_LINE("imul", 2, REG_RETURN, WORD_SIZE_BYTES_STR);
    ASM_LINE("add", 2, REG_RETURN, REG_FREE);
    ASM_LINE("jmp", 1, DEREF_REG(REG_RETURN));

    /*
     * Generating the lookup table:
     *  go through all cases first and keep track of what
     *  case numbers already exist
     *
     *  go through 0 to <MAX_CASE_VALUE> and for every case that
     *  that exists gets mapped to its own label, every other case
     *  will map to the default case label
     */
    
    //lookup already existing cases
    BitTreeNode_t *casesAdded = BitTreeNode_New();
    if (!casesAdded) {
      fprintf(stderr, "Error allocating bit tree for case statements\n");
      return -1;
    }
    
    
    while (curCase) {
      TreeNode_t *caseValue = TreeNode_getChild(curCase, 0);
      do {
        int caseNumber = getConstInteger(caseValue);
        BitTreeNode_AddBitPattern(casesAdded, caseNumber);
        caseValue = caseValue->sibling;
      
      } while (caseValue);
      curCase = curCase->sibling;
    }
    
    //now go through each case statement and generate the look up table
    writeLine(output, true, tableLabel, NULL, "Case Lookup Table", 0);
    for (int i = 0; i <= maxCaseVal; i++) {
      bool exists = BitTreeNode_AddBitPattern(casesAdded, i);
      if (exists) {
        CASE_TAGGED_LABEL(tempLabel, COMMENT_BUF_LEN, TABLE_TAG_FMT, i);
        MULTILINE_DECL(tempLabel, i, DECLS_PER_LINE);
      } else
        MULTILINE_DECL(defaultLabel, i, DECLS_PER_LINE);
    }
    fprintf(output, "\n");
    BitTreeNode_Destroy(casesAdded);

  }
  //end of lookup table generation
  //otherwise, just go through each case, and do a comparison
  else {
    char numbuf[NUM_TO_STR_BUF];
    
    while (curCase) {
      TreeNode_t *caseValue = TreeNode_getChild(curCase, 0);
      
      do {
  int caseNumber = getConstInteger(caseValue);
  snprintf(numbuf, NUM_TO_STR_BUF, "%d", caseNumber);
  ASM_LINE("cmp", 2, REG_RETURN, numbuf);
  CASE_TAGGED_LABEL(tempLabel, COMMENT_BUF_LEN, TABLE_TAG_FMT, caseNumber);
  ASM_LINE("je", 1, tempLabel);
  caseValue = caseValue->sibling;
      } while (caseValue);

      curCase = curCase->sibling;
    }
    

    
    //COMMENT_LINE("Not yet implemented case method!");
    //no comparisons matched, go to default case
    ASM_LINE("jmp", 1, defaultLabel);
  }




  
  /*
   * Write out each case code now
   */
  curCase = cases;
  while (curCase) {
    TreeNode_t *caseValue = TreeNode_getChild(curCase, 0),
      *caseCode = TreeNode_getChild(curCase, 1);

    //cases can have multiple values associated with it
    //so write out all those labels first
    do {
      int caseNumber = getConstInteger(caseValue);
      CASE_TAGGED_LABEL(tempLabel, COMMENT_BUF_LEN, TABLE_TAG_FMT, caseNumber);
      writeLine(output, true, tempLabel, NULL, "case lookup", 0);
      caseValue = caseValue->sibling;      
    } while (caseValue);

    //then write out the code for this case
    if (generateStatement(output, caseCode))
      return -1;

    //exit code
    ASM_LINE("jmp", 1, endCase);
    curCase = curCase->sibling;
  }

  //default case (else)
  writeLine(output, true, defaultLabel, NULL, "Default Case", 0);
  if (defaultCase && generateStatement(output, defaultCase))
    return -1;

  //close up the switch statement
  writeLine(output, true, endCase, NULL, "End of switch", 0);
  return 0;
}


static int generateRelop(FILE *output, TreeNode_t *node) {

  TreeNode_t *left = TreeNode_getChild(node, 0),
    *right = TreeNode_getChild(node, 1);

  if (generateExp(output, left))
    return -1;

  //store left on stack for new return value
  STORE_RESULT(REG_RETURN);

  if (generateExp(output, right))
    return -1;

  STORE_RESULT(REG_RETURN);
  RESTORE_RESULT(REG_FREE);
  RESTORE_RESULT(REG_RETURN);

  //peform the comparison here
  ASM_LINE("cmp", 2, REG_RETURN, REG_FREE);
  CLEAR_REGISTER(REG_RETURN);

  char *instruction = NULL;
  switch (node->token->type) {
  case TOK_EQ:
    instruction = "setz";  
    break;

  case TOK_NOTEQ:
    instruction = "setnz";
    break;
    
  case TOK_LESS:
    instruction = "setl";
    break;
    
  case TOK_GREATER:
    instruction = "setg";
    break;
    
  case TOK_LTEQ:
    instruction = "setle";
    break;

  case TOK_GTEQ:
    instruction = "setge";
    break;

  default:
    break;
  }
  if (instruction)
    ASM_LINE(instruction, 1, REG_RETURN_BYTE);
  else
    COMMENT_LINE(makeComment(NO_OPERATOR, node->token->type));
  
  return 0;
}


static int generateOR(FILE *output, TreeNode_t *node, char *shortLabel) {

  char *shortCircuit = NULL;
  SHORTCIRCUIT_CONDITION(shortCircuit, TOK_KEY_OR, generateOR);

  RESTORE_RESULT(REG_FREE);
  //store r-value
  STORE_RESULT(REG_RETURN);

  //clear REG_RETURN
  CLEAR_REGISTER(REG_RETURN);
  //compare l-value first
  TEST_REGISTER(REG_FREE);
  //store result to REG_RETURN
  ASM_LINE("setne", 1, REG_RETURN_BYTE);
  RESTORE_RESULT(REG_FREE);
  //short circuit the or evaluation if true
  ASM_LINE("jne", 1, shortLabel);
    //now do the same for the next value
  TEST_REGISTER(REG_FREE);
  ASM_LINE("setne", 1, REG_RETURN_BYTE);
  //add label for short circuiting
  if (shortCircuit) {
    writeLine(output, true, shortCircuit, NULL, makeComment(SHORTED_OR), 0);
    free(shortCircuit);
  }
    
  return 0;
}

static int generateSimpExp(FILE *output, TreeNode_t *node) {

  if (node->token->type == TOK_KEY_OR)
    return generateOR(output, node, NULL);
  
  TreeNode_t *left = TreeNode_getChild(node, 0),
    *right = TreeNode_getChild(node, 1);

  if (generateExp(output, left))
    return -1;

  STORE_RESULT(REG_RETURN);

  if (generateExp(output, right))
    return -1;

  switch (node->token->type) {
  case TOK_PLUS:
    //adding is commutative
    RESTORE_RESULT(REG_FREE);
    ASM_LINE("add", 2, REG_RETURN, REG_FREE);
    break;
  case TOK_MINUS:
    //subtraction is not commutative
    //need to swap REG_RETURN with REG_FREE
    ASM_LINE("mov", 2, REG_FREE, REG_RETURN);
    RESTORE_RESULT(REG_RETURN);
    ASM_LINE("sub", 2, REG_RETURN, REG_FREE);
    break;
  case TOK_KEY_OR:
    break;
  }
  return 0;
}

static int generateAND(FILE *output, TreeNode_t *node, char *shortLabel) {


  char *shortCircuit = NULL;
  SHORTCIRCUIT_CONDITION(shortCircuit, TOK_KEY_AND, generateAND);
  
  //get l-value
  RESTORE_RESULT(REG_FREE);
  //store r-value
  STORE_RESULT(REG_RETURN);
  
  //clear REG_RETURN
  CLEAR_REGISTER(REG_RETURN);
  //compare l-value first
  TEST_REGISTER(REG_FREE);
  //store result to REG_RETURN, if equal to 0, short circuit
  ASM_LINE("setne", 1, REG_RETURN_BYTE);
  RESTORE_RESULT(REG_FREE);
  //short circuit if equal to 0
  ASM_LINE("jz", 1, shortLabel);
  //now do the same for the next value
  TEST_REGISTER(REG_FREE);
  writeLine(output, true, NULL, "setne", NULL, 1, REG_FREE_BYTE);
  //peform and
  ASM_LINE("and", 2, REG_RETURN, REG_FREE);
  
  //add label for short circuiting
  if (shortCircuit) {
    writeLine(output, true, shortCircuit, NULL,  makeComment(SHORTED_AND),  0);
    free(shortCircuit);
  }

  return 0;
}

static int generateTerm(FILE *output, TreeNode_t *node) {

  if (node->token->type == TOK_KEY_AND)
    return generateAND(output, node, NULL);

  TreeNode_t *left = TreeNode_getChild(node, 0),
    *right = TreeNode_getChild(node, 1);

  
  if (generateExp(output, left))
    return -1;

  //store left on stack for new return value
  STORE_RESULT(REG_RETURN);

  if (generateExp(output, right))
    return -1;

  //right expression now stored in REG_RETURN
 
  //perform operation
  switch (node->token->type) {
  case TOK_STAR:
    //get left result into free reg, order doesn't matter for multiply
    RESTORE_RESULT(REG_FREE);
    ASM_LINE("imul", 2, REG_RETURN, REG_FREE);
    break;

  case TOK_KEY_MOD:
  case TOK_KEY_DIV:
    //swap eax and ebx for division
    //move right expression into REG_FREE
    ASM_LINE("mov", 2, REG_FREE, REG_RETURN);
    //get left expression into REG_RETURN
    RESTORE_RESULT(REG_RETURN);
    //sign extend eax to edx
    ASM_LINE("cdq", 0);
    //divide REG_RETURN by REG_FREE
    ASM_LINE("idiv", 1, "DWORD "REG_FREE);
    //exit now for division
    if (node->token->type == TOK_KEY_DIV)
      break;

    //otherwise, move remainder to REG_RETURN
    ASM_LINE("mov", 2, REG_RETURN, "edx");
    break;

  
  case TOK_KEY_AND: {

  } break;

  case TOK_KEY_SHR:
    //swap eax and ebx for shifting so that eax = eax >> ebx
    ASM_LINE("mov", 2, REG_SHIFT, REG_RETURN_BYTE);
    RESTORE_RESULT(REG_RETURN);
    ASM_LINE("sar", 2, REG_RETURN, REG_SHIFT);
    break;

  case TOK_KEY_SHL:
    //swap eax and ebx for shifting so that eax = eax << ebx
    ASM_LINE("mov", 2, REG_SHIFT, REG_RETURN_BYTE);
    RESTORE_RESULT(REG_RETURN);
    ASM_LINE("sal", 2, REG_RETURN, REG_SHIFT);
    break;

  default:
    fprintf(stderr, "Missing operator?\n");
    return -1;
  }
  
  return 0;
}

static int generateNot(FILE *output, char *value) {

  COMMENT_LINE(makeComment(LOG_NEG, value));
  //eax - 0
  TEST_REGISTER(REG_RETURN);
  
  //clear eax, since we'll only be setting the bool result in the
  //first byte
  CLEAR_REGISTER(REG_RETURN);

  //if CC is 0, indicating eax is 'fase', set eax to !false
  ASM_LINE("sete", 1, REG_RETURN_BYTE);
  //otherwise, eax stays 0 for !true
  return 0;
}


//loads value of variable to eax and address of variable to ecx
static int generateVariable(FILE *output, TreeNode_t *node) {
  
  //load variable to return register
  char buffer[NUM_TO_STR_BUF];
  
  //calculate how many bytes we need to look behind to add to
  //the relative offset stored in the symbol
  int stackOffset = 0;
  SymTable_findAll(currentScope, node->entry->key, &stackOffset);

  //offsets started at 0, so add 1 word to get the proper position
  int varOffset = node->entry->stackOffset + WORD_SIZE_BYTES;  
  stackOffset += -varOffset;
  
  //look up the stack offset for the variable
  snprintf(buffer, NUM_TO_STR_BUF, STACK_VAR_FMT, stackOffset);

  if (TreeNode_hasType(node, ARRAY)) {
    COMMENT_LINE(makeComment(ARRAY_INDEX, node->entry->key));
    //evaluate array indexing size
    if (generateExp(output, TreeNode_getChild(node, 0)))
      return -1;

    //convert offset size into bytes
    ASM_LINE("imul", 2, REG_RETURN, WORD_SIZE_BYTES_STR);
    //offset stored in eax...
    STORE_RESULT(REG_RETURN);

    //load array base address to add onto
    ASM_LINE("lea", 2, REG_VARADDR, buffer);
    RESTORE_RESULT(REG_FREE);
    //add index offset to array address
    ASM_LINE("sub", 2, REG_VARADDR, REG_FREE);
  } else {
    COMMENT_LINE(makeComment(LOAD_VAR, node->entry->key));
    ASM_LINE("lea", 2, REG_VARADDR, buffer);
  }

  ASM_LINE("mov", 2, REG_RETURN, DEREF_REG(REG_VARADDR));
  //check if we need to negate the value
  if (TreeNode_hasType(node, NOT))
    generateNot(output, node->entry->key);

  return 0;
}

static int generateConstant(FILE *output, TreeNode_t *node) {

  char numbuffer[NUM_TO_STR_BUF];
  if (node->entry) {
    //dealing with a constant/literal string in the rodata table
    //copy string address to REG_RETURN
    if (Symbol_hasType(node->entry, SYMTYPE_INT)) {
      snprintf(numbuffer, NUM_TO_STR_BUF, "%d", node->entry->data.value);
      ASM_LINE("mov", 2, REG_RETURN, numbuffer);
    }
    else
      ASM_LINE("mov", 2, REG_RETURN, node->entry->key);
    
    return 0;
  }
  
  //otherwise, we are dealing with a literal integer 
  snprintf(numbuffer, NUM_TO_STR_BUF, "%d", node->token->lexeme.value);
  ASM_LINE("mov", 2, REG_RETURN, numbuffer);

  if (TreeNode_hasType(node, NOT))
    generateNot(output, numbuffer);
 
  return 0;
}



int generateExp(FILE *output, TreeNode_t *node) {

  unsigned long long type = node->type & EXP_FILTER;
  
  switch (type) {
  case NODETYPE_BIT(RELOP):
    TreeNode_rmType(node, RELOP);
    return generateRelop(output, node);

  case NODETYPE_BIT(BINOP):
    return generateSimpExp(output, node);

  case NODETYPE_BIT(MULOP):
    return generateTerm(output, node);

  case NODETYPE_BIT(VARIABLE):
    return generateVariable(output, node);

  case NODETYPE_BIT(CONSTANT):
    return generateConstant(output, node);

  case NODETYPE_BIT(UNARYOP):
    if (generateExp(output, TreeNode_getChild(node, 0)))
      return -1;

    //unary - sign, make value negative
    if (node->token->type == TOK_MINUS)
      ASM_LINE("neg", 1, REG_RETURN);
    return 0;

  }

  return 0;
}


int generateStatement(FILE *output, TreeNode_t *node) {

  //  TreeNode_printNode(stdout, node, false); 
  if (!node) {
    fprintf(stderr, "Null statement\n");
    return 0;
  }
  unsigned long long type = node->type & STMT_FILTER;
  
  switch (type) {
  case NODETYPE_BIT(STMT_LIST):{
    //while in statement list, go through all statement siblings
    TreeNode_t *stmts = TreeNode_getChild(node, 0);

    do {
      if (generateStatement(output, stmts))
        return -1;

      stmts = stmts->sibling;
    } while (stmts);
    
  } break;
  case NODETYPE_BIT(READ_STMT):
    return generateReadStmt(output, node);
    
  case NODETYPE_BIT(WRITE_STMT):
    return generateWriteStmt(output, node);

  case NODETYPE_BIT(CASE_STMT):
    return generateCaseStmt(output, node);

  case NODETYPE_BIT(WHILE_STMT):
    return generateWhileStmt(output, node);
  
  case NODETYPE_BIT(IF_STMT):
    return generateIfStmt(output, node);
    
  case NODETYPE_BIT(ASSIGN_STMT):
    return generateAssignStmt(output, node);

  case NODETYPE_BIT(BLOCK_STMT):
    return generateBlockStmt(output, node);

  default:
    break;
  }


  return 0;
}


//action to perform when going through the rodata hash table
static int writeStrConst(Symbol_t *symbol, void *data) {
  
  FILE *output = (FILE *)data;
  writeLine(output, false, symbol->key, "dd", NULL, 0);
  //some strings can be much longer than what writeLine intends
  //to handle on a general basis
  fprintf(output, "%s,0\n", symbol->data.string);
  return 0;
}

static int writeASMHeader(FILE *output) {
  fprintf(output, FILE_HEADER);
  COMMENT_LINE("io library definitions");
  writeLine(output, true, NULL, "extern", NULL, 2, "read", "write");
  COMMENT_LINE("define main function");
  writeLine(output, true, NULL, "global", NULL, 1, MAIN_LABEL);
  
  return 0;
}


static int writeReadOnlyData(FILE *output, SymTable_t *rodata) {

  BLANK_LINE;
  SECTION_LINE(".rodata");
  return SymTable_forEach(rodata, (void *)output, writeStrConst);
}

static int writeTextSection(FILE *output, TreeNode_t *ast) {
  BLANK_LINE;
  SECTION_LINE(".text");
  writeLine(output, true, MAIN_LABEL, NULL, NULL, 0);
  generateStatement(output, ast);

  EXIT_PRGM;
  return 0;
}

int CodeGen_process(FILE *output, TreeNode_t *ast, SymTable_t *rodata) {
  
  //print the header for the assembly file
  if (writeASMHeader(output)) {
    fprintf(stderr, "Error writing ASM File header\n");
    return -1;
  }
  
  if (writeReadOnlyData(output, rodata)) {
    fprintf(stderr, "Error writing read only data section\n");
    return -1;
  }
  
  if (writeTextSection(output, ast)) {
    fprintf(stderr, "Error generating ASM Text section\n");
    return -1;
  }

  return 0;
}
