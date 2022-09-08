/*
 * CMPT 399 (Winter 2016)
 * Assignment 4: Code Generation
 * Author: Derrick Gold
 *
 * BitTree API
 */
#include <stdio.h>
#include <stdlib.h>
#include "bittree.h"

/*
 * Returns the number of used bits in a pattern
 * For example, a pattern with a decimal value of '1' only 
 * uses 1 bit to represent it. Decimal '2' requires 2 bits
 * to represent its binary form in text '0b10'.
 */
static int getUsedBitCount(intptr_t bitPattern) {
  
  size_t maxBits = sizeof(intptr_t) << 3;
  size_t bitCount = maxBits;
  
  //get the number of used bits in the pattern
  for (int curBit = maxBits - 1; curBit > 0; curBit--) {
    if (bitPattern & (1ULL << curBit))
      break;

    bitCount--;
  }

  return bitCount;
}

BitTreeNode_t *BitTreeNode_New(void) {

  BitTreeNode_t *newNode = calloc(1, sizeof(BitTreeNode_t));

  if (!newNode) {
    fprintf(stderr, "Error allocating new bit tree node\n");
    return NULL;
  }

  return newNode;
}



bool BitTreeNode_AddBitPattern(BitTreeNode_t *head, intptr_t bitPattern) {

  bool found = true;
  
  if (bitPattern == 0) {
    found = head->exists;
    head->exists = true;
    return found;
  }
  
  size_t bitCount = getUsedBitCount(bitPattern);
  
  BitTreeNode_t *curNode = head;
  for (int curBit = 0; curBit <= bitCount; curBit++) {
   
    if (curBit == bitCount) {
      found = curNode->exists;
      curNode->exists = true;
      break;
    }
    
    bool bitStatus = bitPattern & (1ULL << curBit);
    
    //follow through bit tree until we come across
    //a bit pattern that hasn't been set
    if (!curNode->bit[bitStatus]) {
      found = false;
      BitTreeNode_t *nextNode = BitTreeNode_New();
      curNode->bit[bitStatus] = nextNode;
      curNode = nextNode;
    } else {
      curNode = curNode->bit[bitStatus];
    }
  }

  return found;
}

void BitTreeNode_Destroy(BitTreeNode_t *head) {

  if (head == NULL)
    return;

  for (int i = 0; i < 2; i++) {

    if (head->bit[i])
      BitTreeNode_Destroy(head->bit[i]);
  }

  free(head);
  return;
}

