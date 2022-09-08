/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * BitTree API
 * 
 * This bit tree structure is designed to check for duplicate
 * numerical values. The Bit tree is essentially a decision
 * tree where each decision is made on the status
 * of a bit in a bitPattern (number).
 * Look up complexity is O(n) where n is the number of used
 * bits in the pattern to lookup. 
 */

#ifndef __BIT_TREE_H__
#define __BIT_TREE_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct BitTreeNode_s {

  struct BitTreeNode_s *bit[2];
  bool exists;
  
} BitTreeNode_t;

/*
 * BitTreeNode_New:
 *  Allocate a new Bit Tree node
 *
 * Returns:
 *  New BitTreeNode_t ptr on success. NULL if the allocation
 *  fails.
 */
BitTreeNode_t *BitTreeNode_New(void);

/*
 * BitTreeNode_addBitPattern:
 *  Add a pattern of bits to the bit tree if it doesn't exist.
 *
 * Arguments:
 *  head: The first entry of the bit tree to add to.
 *  bitPattern: an intptr_t sized integer containing a pattern
 *    of set bits to add.
 *
 * Returns:
 *  True if bitPattern already exists in the tree. False if the pattern
 *  did not exist and was added.
 */
bool BitTreeNode_AddBitPattern(BitTreeNode_t *head, intptr_t bitPattern);

/*
 * BitTreeNode_destroy:
 *  Frees all memory associated with a bit tree.
 *
 * Arguments:
 *  head: The first entry of the bit tree to free.
 */
void BitTreeNode_Destroy(BitTreeNode_t *head);

#endif //__BIT_TREE_H__
