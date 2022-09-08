/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * Abstract Syntax Tree API.
 *
 */
#ifndef LEXER_TREE_H
#define LEXER_TREE_H

#include <stdbool.h>
#include "lexer.h"
#include "symtab.h"

//The number of children each node can have
#define TREENODE_CHILD_MAX 3

//Get a count of the maximum number of (usable) bits in a NodeType value
#define NODETYPE_BITS_COUNT (sizeof(NodeType) << 3)

//Get bit mask for 'x' bit
#define NODETYPE_BIT(x) (1ULL << (x))

/*
 * NodeType in the node structure stores each of the listed type 
 * in their own bit. These enum values represent their
 * bit number rather than the actual bit value they are modifying.
 * 
 * This is done so that the string for each type can still be
 * accessed in NodeTypeText through NodeTypeText[bitNum],
 * where bitNum refers to any entry (except MAKE64) in this enum.
 *
 * Type assignment for the node is done using the NODETYPE_BIT macro:
 *              node->type |= NODETYPE_BIT(bitNum) 
 * where bitNum refers to any entry (except MAKE64) in this enum.
 *
 * To check if a node is of a certain type, one just needs to check
 * the return value of:
 *              node->type & NODETYPE_BIT(bitNum)
 * where bitNum refers to any entry (except MAKE64) in this enum.
 *
 * The TreeNode_addType, TreeNode_rmType, and TreeNode_hasType functions 
 * make up the API for dealing with TreeNode types.
 */
typedef enum {
  BLOCK = 0,
  CONST_SEC,
  CONST_DECL,
  VAR_SEC,
  VAR_DECL,
  VAR_LIST,
  TYPE,
  STMT_LIST,
  STMT,
  NULL_STMT,
  ASSIGN_STMT,
  BLOCK_STMT,
  IF_STMT,
  WHILE_STMT,
  CONDITION,
  CASE_STMT,
  CASE_LIST,
  CASE,
  CONST_LIST,
  WRITE_STMT,
  READ_STMT,
  EXP,
  SIMPEXP,
  FACTOR,
  RELOP,
  UNARYOP,
  BINOP,
  MULOP,
  CONSTANT,
  VARIABLE,
  SIMP_NAME,
  TERM,
  // Extra node types
  NOT,
  // 64 bit time!
  LITERAL,
  ARRAY,
  INTEGER,
  STRING,
  
  //make NodeType into type uint64_t
  MAKE64 = NODETYPE_BIT(63)
} NodeType;

//external reference to NodeType strings
extern const char *NODE_TYPE_TEXT[];

/*
 * Small enum for resolving what value a node will return.
 * This is used for type checking.
 */
typedef enum {
  RETURN_NONE,
  RETURN_ERR,
  RETURN_INT,
  RETURN_BOOL,
  RETURN_STR,
} NodeReturnType;

extern const char *NODE_RETURNTYPE_TEXT[];

/*
 * TreeNode_t:
 *  TreeNode_t (struct TreeNode_s) defines a node for use
 *  in an abstract syntax tree. 
 *
 *  Each node can have multiple types associated with it when
 *   appropriate. This is useful for typecast one node type to
 *   another supported node type, such as a 'constant' type, to a 
 *  'variable' type. The method in which these types are stored 
 *  (As described above the NodeType enum allows for easy look up
 *   and  assignment without the need for a complex data structure.
 *  I'm not sure what is going to be required of the semantic analysis,
 *  so I figure it might be easier to store the node's type history as
 *  it makes its way into the tree.
 *
 *  Each node has the potential to store a token with it, where
 *  some nodes are used as structural nodes in maintaining the
 *  relationship of the token nodes. The token property will
 *  be set to NULL, if there are no tokens associated with it.
 *
 *  isSibling and isChild properties are boolean values to indiciate
 *  if the node is (true) a sibling node, child node, or head node 
 *  (both sibling & child set to false). It's main purpose is for 
 *  describing node relationships within the printed output of
 *  the created abstract syntax tree.
 *
 *  Sibling nodes essentially make the current node into
 *  a form of a linked list, in which the siblings tagged on
 *  are the same level of evaluation as the current node, and
 *  must be required for the parent node of the current node.
 *  A node can have any number of siblings.
 *
 *  In traversing this tree structure, for each node, the children
 *  nodes must be followed first, and then the sibling node is
 *  followed with the process repeating.
 *
 */
typedef struct TreeNode_s {
  NodeType type;
  char numChild;
  
  bool isSibling, isChild;
  LexToken_t *token;
  struct TreeNode_s *sibling;
  struct TreeNode_s *child[TREENODE_CHILD_MAX];

  //Points to the symbol table if node is a block.
  //Is always null otherwise.
  SymTable_t *symbols;
  //  SymTable_t *localSym;

  /*
   * If node refers to an identifier, it will point to
   * its own entry in the symbol table
   */
  Symbol_t *entry;
  
  /*
   * What type of value this node returns
   *  Determined through semantic analysis
   */
  NodeReturnType returns;

  /*
   * Number of arguments if this node is a Read or Write
   * statement node. If this is a Case statement node, this
   * number will indicate the number of cases that exist.
   */
  int argc;
  
} TreeNode_t;

/*
 * TreeNode_newNode:
 *  Instantiate a new tree node.
 *
 * Arguments:
 *  type: What type of node to create (use bit number as defined by elements
 *        of the NodeType enumerator.
 *  token: A lexer token to assign to the node. NULL if no token is required.
 *
 * Returns:
 *  NULL if the node failed to be created. Otherwise a pointer
 *  to the newly created TreeNode_t instance.
 */
TreeNode_t *TreeNode_newNode(NodeType type, LexToken_t *token);

/*
 * TreeNode_destroyNode:
 *  Frees up all memory associated with a node.
 *
 * Arguments:
 *  node: The node to delete
 */
void TreeNode_destroyNode(TreeNode_t *node);

/*
 * TreeNode_destroy:
 *  Frees up all memory associated with an abstract syntax tree.
 *
 * Arguments:
 *  root: The root node of the abstract syntax tree to free.
 */
void TreeNode_destroy(TreeNode_t *root);


TreeNode_t *TreeNode_setChild(TreeNode_t *parent, TreeNode_t *child, unsigned char childPos);

/*
 * TreeNode_addSibling:
 *  Add a sibling to a specific node.
 *
 * Arguments:
 *  start: The node to add a sibling to.
 *  sibnode: the sibling node to add to the start node.
 *
 * Returns:
 *  A pointer to the sibling node added on success. NULL if a NULL 
 *  value is passed into either start or sibnode arguments.
 */
TreeNode_t *TreeNode_addSibling(TreeNode_t *start, TreeNode_t *sibnode);

/*
 * TreeNode_traverse:
 *  Perform an operation on each node in the tree, in the proper
 *  node order appropriate wih regard to maintaining syntax relationships.
 *
 * Arguments:
 *  depth: The current depth to start counting on. Should typically start on depth 0.
 *  root: The root or starting point of the tree to explore.
 *  data: Any data that may need to be passed to the functions called pre, and post node visit.
 *  pre: A function to call on a node, prior to visiting its children and siblings
 *  post: A function to call on a node after visiting its children and siblings
 *
 *  If post or pre functions return a value other than 0, the tree traversal will
 *  stop immediately.
 *
 * Returns:
 *  0 if all nodes have been traversed. Anything else indicates the tree traversal was
 *  interrupted by a pre/post node visit action. 
 */
int TreeNode_traverse(int depth, TreeNode_t *root, void *data, int (*pre)(int, TreeNode_t *, void *),
                         int (*post)(int, TreeNode_t *, void *));

/*
 * TreeNode_printNode:
 *  Print out the details of a node.
 *
 * Arguments:
 *  output: The file stream to write node info to.
 *  node: The node to print details of.
 *  symtable: Set to true to print out any symbol table associated with
 *              the node.
 */
void TreeNode_printNode(FILE *output, TreeNode_t *node, bool symtable);

/*
 * TreeNode_print:
 *  Print out all the nodes in a tree.
 *
 * Arguments:
 *  output: The file stream to write node info to.
 *  head: The root node of the tree to print.
 *  symtab: Set true to print out attached symbol table
 */
void TreeNode_print(FILE *output, TreeNode_t *head, bool symtab);

/*
 * TreeNode_addType:
 *  Assign a node type (a bit number defined in NodeType enumerator)
 *  to a node.
 *
 * Arguments:
 *  node: The node to assign a type to.
 *  type: the NodeType bit number indicate what type to assign.
 */
void TreeNode_addType(TreeNode_t *node, NodeType type);

/*
 * TreeNode_rmType:
 *  Remove a type that has already been assigned to a node.
 *
 * Arguments:
 *  node: The node to remove a type from.
 *  type: the NodeType bit number indicate what type to remove.
 */
void TreeNode_rmType(TreeNode_t *node, NodeType type);

/*
 * TreeNode_hasType:
 *  Check if a node has been assigned a specific node type.
 *
 * Arguments:
 *  node: The node to check if a type has been assigned.
 *  type: the NodeType bit number indicate what type to check for.
 *
 * Returns:
 *  True if the node has the requested type assigned to it. False
 *  otherwise, or if a NULL value has been passed into the node
 *  argument.
 */
bool TreeNode_hasType(TreeNode_t *node, NodeType type);

/*
 * TreeNode_getChild:
 *  Get a specific child node from a parent node.
 *
 * Arguments:
 *  parent: parent node to get child node from.
 *  childNum: Which child to get by index. Min value 0, max of 
 *    TREENODE_CHILD_MAX.
 *
 * Returns:
 *  TreeNode_t pointer the specified child. Returns
 *  NULL if child does not exist.
 */
TreeNode_t *TreeNode_getChild(TreeNode_t *parent, int childNum);

/*
 * TreeNode_getSymTable:
 *  Get the symbol table a node is referencing.
 *
 * Arguments:
 *  node: The node to get the symbol table of
 *
 * Returns:
 *  SymTable_t pointer to the nodes table. Returns
 *  NULL if there is no table associated with this node.
 */
SymTable_t *TreeNode_getSymTable(TreeNode_t *node);

/*
 * TreeNode_setSymTable:
 *  Set the symbol table for a node to reference.
 *
 * Arguments:
 *  node: The node to add the table reference to.
 *
 */
void TreeNode_setSymTable(TreeNode_t *node, SymTable_t *table);

/*
 * TreeNode_setReturnType:
 *  Set the calculated return type of a node. This is used
 *  for semantic analysis by allowing a node to save its
 *  typed return status for other checks to refer to.
 *
 * Arguments:
 *  node: The node to set the return type for.
 *  type: The type of data the node is supposed to return.
 *    Can be one of the following:
 *      RETURN_NONE (Default node return type)
 *      RETURN_INT : 32 bit integer
 *      RETURN_BOOL : Boolean value
 *      RETURN_STR : string
 *
 */
void TreeNode_setReturnType(TreeNode_t *node, NodeReturnType type);

/*
 * TreeNode_getReturnType:
 *  Get the data type a node is expected to return.
 *
 * Arguments:
 *  node: The node to check the return type of.
 *
 * Returns:
 *  RETURN_NONE, RETURN_INT, RETURN_BOOL, or RETURN_STR.
 */
NodeReturnType TreeNode_getReturnType(TreeNode_t *node);

/*
 * TreeNode_setSymbolRef:
 *  Assign a symbol reference to a specified node. 
 *  This is used for semantic checks as it allows
 *  identifier nodes to point to their value and or 
 *  return type that resides in a symbol table.
 *
 * Arguments:
 *  node: The node to assign the symbol reference to.
 *  symbol: The symbol to assign to the node, usually a
 *    value that is returned by a symbol table look up.
 *
 */
void TreeNode_setSymbolRef(TreeNode_t *node, Symbol_t *symbol);

/*
 * TreeNode_getSymbolRef:
 *  Get the symbol a node is referencing from a
 *  symbol table.
 *
 * Arguments:
 *  node: The node to get the symbol of
 *
 * Returns:
 *  A pointer to a symbol instance. NULL if there
 *  is no symbol associated with the node.
 */
Symbol_t *TreeNode_getSymbolRef(TreeNode_t *node);

/*
 * TreeNode_setArgCount:
 *  Store an important integer value with a node.
 *  Typically with the context of a child node count:
 *  argument count for Read/Write statements or case
 *  counts for Case statements.
 *
 * Arguments:
 *  node: The node to set the argument count of
 *  argc: integer value of arguments/nodes to store in node.
 */
void TreeNode_setArgCount(TreeNode_t *node, int argc);

/*
 * TreeNode_getArgCount:
 *  Get a count of important nodes within the current node.
 *
 * Arguments:
 *  node: The node to get the node count of.
 *
 * Returns:
 *  Integer number of nodes the current node may deem
 *  as important. -1 if there are no node counts.
 */
int TreeNode_getArgCount(TreeNode_t *node);

#endif //LEXER_TREE_H
