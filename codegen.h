/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Code Generation from an abstract syntax tree, produces x86
 * assembly code.
 */

#ifndef __CODEGEN_H__
#define __CODEGEN_H__

#include "symtab.h"
#include "tree.h"

int CodeGen_process(FILE *file, TreeNode_t *ast, SymTable_t *rodata);


#endif //__CODEGEN_H__
