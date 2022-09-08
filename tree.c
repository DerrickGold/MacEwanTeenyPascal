/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Abstract Syntax Tree API.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

/*
 * Set the number of spaces to indent
 * each child level when printing the tree.
 */
#define DEPTH_TAB_SIZE 4

#define CASE_ARGC_TEXT "Case Count: %d\n"
#define ARGC_TEXT "Arguments: %d\n"

//Node type strings for printing
const char *NODE_TYPE_TEXT[] = {
  "Block",
  "Const Section",
  "Const Declaration",
  "Variable Section",
  "Variable Declaration",
  "Variable List",
  "Type",
  "Statement List",
  "Statement",
  "Null Statement",
  "Assign Statement",
  "Block Statement",
  "If Statement",
  "While Statement",
  "Condition",
  "Case Statement",
  "Case List",
  "Case",
  "Const List",
  "Write Statement",
  "Read Statement",
  "Expression",
  "Simple Expression",
  "Factor",
  "Relational Operator",
  "Unary Operator",
  "Binary Adding Operator",
  "Multiply Operator",
  "Constant",
  "Variable",
  "Simple Name",
  "Term",
  "Not",
  "Literal",
  "Array",
  "Integer",
  "String",
};

const char *NODE_RETURNTYPE_TEXT[] = {
  "None",
  "Error",
  "Integer",
  "Boolean",
  "String",
};

/*
 * Find the last sibling of a node.
 */
static TreeNode_t *lastSibling(TreeNode_t *start) {

  TreeNode_t *next = start;
  while (next->sibling)
    next = next->sibling;

  return next;
}

/*
 * Set a child node in a parent node.
 */
TreeNode_t *TreeNode_setChild(TreeNode_t *parent, TreeNode_t *child, unsigned char childPos) {
  
        //make sure a child and parent node have been provided
  if (!child || !parent)
    return NULL;
  
  //parent can't support more than REENODE_CHILD_MAX childs
  if (childPos >= TREENODE_CHILD_MAX || childPos < 0)
    return NULL;
  
  parent->child[(unsigned int)childPos] = child;
  //indicate the node we are adding is a child node
  child->isChild = true;
  return child;
}


/*
 * Helper function for TreeNode_destory. It gets passed into
 * a TreeNode_traverse function call.
 */
static int deleteHelper(int depth, TreeNode_t *node, void *data) {

  TreeNode_destroyNode(node);
  //keep traversing the tree
  return 0;
}

/*
 * Helper function for TreeNode_print. It gets passed into
 * a TreeNode_traverse function call.
 */
static int traversePrint(int depth, TreeNode_t *node, void *data) {

  //set the indentation
  printf("%-*s", depth * DEPTH_TAB_SIZE, "");
  //print the node.
  TreeNode_printNode((FILE*) data, node, true);
  //keep traverseing the tree
  return 0;
}


TreeNode_t *TreeNode_newNode(NodeType type, LexToken_t *token) {

  TreeNode_t *node = calloc(1, sizeof(TreeNode_t));
  if (!node)
    return NULL;
  
  node->type = NODETYPE_BIT(type);
  node->numChild = 0;
  node->token = token;
  node->argc = -1;
  
  return node;
}


TreeNode_t *TreeNode_addSibling(TreeNode_t *start, TreeNode_t *sibnode) {
  
  if (!start || !sibnode)
    return NULL;

  //get the last node of the start nodes siblings
  TreeNode_t *lastSib = lastSibling(start);
  //add sibling to the last node
  lastSib->sibling = sibnode;

  //mark the sibling node as a sibling
  sibnode->isSibling = true;
  return sibnode;
}


void TreeNode_destroyNode(TreeNode_t *node) {

  if (!node)
    return;
  
  //free token data if necessary
  if (node->token)
    Lexer_tokenDestructor(node->token);

  if (node->symbols)
    SymTable_destroy(node->symbols);  
  
  memset(node, 0, sizeof(TreeNode_t));
  free(node);
}


int TreeNode_traverse(int depth, TreeNode_t *root, void *data, int (*pre)(int, TreeNode_t *, void *),
          int (*post)(int, TreeNode_t *, void *)) {
  
  if (root == NULL)
    //return 0 for all ok
    return 0;

  if (pre) {
    int rtrn = pre(depth, root, data);
    if (rtrn)
      return rtrn;
  }

  //visit all children first
  int i = 0;
  for (i = 0; i < TREENODE_CHILD_MAX; i++) {

    if (!root->child[i])
      continue;
    
    int rtrn = TreeNode_traverse(depth + 1, root->child[i], data, pre, post);
    //allow tree to short circuit if pre or post return non-zero value
    if (rtrn)
      return rtrn;
  }


  //save sibling pointer incase post op is freeing nodes
  TreeNode_t *sibling = root->sibling;
  
  if (post) {
    int rtrn = post(depth, root, data);
    if (rtrn)
      return rtrn;
  }
  //once all the children have been visited, visit the sibling
  return TreeNode_traverse(depth, sibling, data, pre, post);
}


void TreeNode_destroy(TreeNode_t *root) {
  
  TreeNode_traverse(0, root, NULL, NULL, deleteHelper);
}


void TreeNode_printNode(FILE *output, TreeNode_t *node, bool symtable) {

  if (!node || !output)
    return;

  //print out the node's relation
  if (node->isSibling)
    fprintf(output, "[Sibling] ");
  if (node->isChild)
    fprintf(output, "[Child] ");
  
  if (!(node->isSibling | node->isChild))
    fprintf(output, "[Head] ");

  //print out the token if available
  if (node->token) {
    Lexer_printToken(*node->token, output);
    fprintf(output, ", ");
  }
  
  //print out all the types associated with the node
  int bit = 0;
  NodeType oldType = node->type;
  
  fprintf(output, "(");
  for (bit = 0; bit < NODETYPE_BITS_COUNT && oldType > 0; bit++) {
    if (!TreeNode_hasType(node, bit))
      continue;

    //unset the type
    oldType &= ~NODETYPE_BIT(bit);

    //check if we are on the last set type
    if (!oldType)
      fprintf(output, "%s", NODE_TYPE_TEXT[bit]);
    else
      fprintf(output, "%s, ", NODE_TYPE_TEXT[bit]);
  }
  fprintf(output, ") ");

  //if there is an argc value, print it as well
  int argc = TreeNode_getArgCount(node);
  if (argc > 0) {
    char *fmt = ARGC_TEXT;
    //for case statements, argc refers to the number of
    //cases exist in it. So change the text accordingly.
    if (TreeNode_hasType(node, CASE_STMT))
      fmt = CASE_ARGC_TEXT;
    
    fprintf(output, fmt, argc);
  }
  else
    fprintf(output, "\n");

  //Print table
  if (symtable) {
    SymTable_t *table = TreeNode_getSymTable(node);
    if (table)
      SymTable_print(output, table);
  }
}


void TreeNode_print(FILE *output, TreeNode_t *head, bool symtable) {
  
  TreeNode_traverse(0, head, (void *)output, traversePrint, NULL);
}


TreeNode_t *TreeNode_getChild(TreeNode_t *parent, int childNum) {

  if (!parent || childNum < 0 || childNum > TREENODE_CHILD_MAX)
    return NULL;

  return parent->child[childNum];
}

void TreeNode_addType(TreeNode_t *node, NodeType type) {
  
  if (!node)
    return;
  
  node->type |= NODETYPE_BIT(type);
}

void TreeNode_rmType(TreeNode_t *node, NodeType type) {

  if (!node)
    return;
  
  node->type &= ~NODETYPE_BIT(type);
}

bool TreeNode_hasType(TreeNode_t *node, NodeType type) {

  if (!node)
    return false;
  
  return node->type & NODETYPE_BIT(type);
}

SymTable_t *TreeNode_getSymTable(TreeNode_t *node) {

  if (!node)
    return NULL;

  return node->symbols;
}

void TreeNode_setSymTable(TreeNode_t *node, SymTable_t *table) {

  if (!node)
    return;

  node->symbols = table;
}

void TreeNode_setReturnType(TreeNode_t *node, NodeReturnType type) {

  if (!node)
    return;

  node->returns = type;
}


NodeReturnType TreeNode_getReturnType(TreeNode_t *node) {

  if (!node)
    return RETURN_NONE;

  return node->returns;
}

void TreeNode_setSymbolRef(TreeNode_t *node, Symbol_t *symbol) {

  if (!node || !symbol)
    return;

  node->entry = symbol;
}

Symbol_t *TreeNode_getSymbolRef(TreeNode_t *node) {

  if (!node)
    return NULL;

  return node->entry;
}

void TreeNode_setArgCount(TreeNode_t *node, int argc) {

  if (!node)
    return;

  node->argc = argc;
}

int TreeNode_getArgCount(TreeNode_t *node) {

  if (!node)
    return -1;

  return node->argc;
}



