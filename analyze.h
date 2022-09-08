/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Semantic Analysis Functions
 */
#ifndef __ANALYZE_H__
#define __ANALYZE_H__

#include "tree.h"

//These literals match with strings defined
//as 'SEMANTIC_ERROR_MSGS' in analyze.c
typedef enum {
  NONE = 0,
  REDECLARED_ID,
  UNDECLARED_ID,
  INVALID_ARRAY_DECL,
  UNEXPECTED_R_TYPE,
  INVALID_L_TYPE,
  UNEXPECTED_L_TYPE,
  INVALID_CONDITION,
  ARRAY_MISSING_INDEX,
  ARRAY_UNDER_BOUNDS,
  ARRAY_OVER_BOUNDS,
  DUPLICATE_CASE,
  VAR_INDEXING,
  NOT_VALUE,
} SemanticErrors;


/*
 * Analyze_semantics:
 *  Performs semantic checks on a generated abstract syntax tree.
 *
 * Arguments:
 *  treeHead: Root node of the abstract syntax tree
 *  verbose: set true to print out symbol tables and updated ast.
 *
 * Returns:
 *  0 on success, -1 if errors occurred.
 */
int Analyze_Semantics(TreeNode_t *treeHead, bool verbose);

/*
 * Analyze_getStatus:
 *  Get more detailed status from the semantics analysis performed
 *  by calling 'Analyze_semantics'.
 *
 * Returns:
 *  Semantic error number.  
 */
SemanticErrors Analyze_GetStatus(void);


SymTable_t *Analyze_GetRodata(void);

void Analyze_Cleanup(void);

#endif //end of __ANALYZE_H__
