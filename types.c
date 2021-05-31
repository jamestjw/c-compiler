#include "defs.h"
#include "gen.h"
#include "misc.h"
#include "tree.h"

// Given two primitive types, return true if they are compatible.
// Sets *left and *right to either 0 or A_WIDEN depending on whether
// one has to be widened to match the other.
//
// onlyright - Only widen left to right.
int type_compatible(int *left, int *right, int onlyright) {
  int leftsize, rightsize;

  // Same types are compatible
  if (*left == *right) { *left = *right = 0; return 1;  }

  // Get the sizes for each type
  leftsize = genprimsize(*left);
  rightsize = genprimsize(*right);

  // Types with zero size are not compatible
  // with anything
  if ((leftsize == 0) || (rightsize == 0)) return 0;

  // Widen types as required
  if (leftsize < rightsize) {
    *left = A_WIDEN;
    *right = 0;
    return 1;
  }

  if (rightsize > leftsize) {
    if (onlyright) return 0;

    *left = 0;
    *right = A_WIDEN;
    return 1;
  }

  // Anything else is compatible
  *left = *right = 0;
  return 1;
}

int pointer_to(int type) {
  int newtype;
  switch (type) {
    case P_VOID: newtype = P_VOIDPTR; break;
    case P_CHAR: newtype = P_CHARPTR; break;
    case P_INT: newtype = P_INTPTR; break;
    case P_LONG: newtype = P_LONGPTR; break;
    default:
      fatald("Invalid pointer of type", type);
      exit(1);
  }

  return newtype;
}

int value_at(int type) {
  int newtype;
  switch (type) {
    case P_VOIDPTR: newtype = P_VOID; break;
    case P_CHARPTR: newtype = P_CHAR; break;
    case P_INTPTR: newtype = P_INT; break;
    case P_LONGPTR: newtype = P_LONG; break;
    default:
      fatald("Unrecognised pointer to type", type);
      exit(1);
  }

  return newtype;
}

int inttype(int type) {
  if (type == P_CHAR || type == P_INT || type == P_LONG)
    return 1;
  return 0;
}

// Return true if the type is of a pointer type
int ptrtype(int type) {
  if (type == P_VOIDPTR || type == P_CHARPTR || 
      type == P_INTPTR || type == P_LONGPTR)
    return 1;
  return 0;
}

// Given an AST tree and a type that we wish for it to become,
// the function attempts to modify the tree by widening or scaling
// to make it compatible with the given type. This function either 
// returns the original tree if no changes were necessary, a 
// modified tree or NULL (if the tree is not compatible with the given
// type).
//
// `op` should not be zero if this tree will be a part of a binary
// operation.
struct ASTnode *modify_type(struct ASTnode *tree, int rtype, int op) {
  int ltype;
  int lsize, rsize;

  ltype = tree->type;

  // Handle integer types
  if (inttype(ltype) && inttype(rtype)) {
    // Check if types already match
    if (ltype == rtype) return tree;

    // Get the sizes of each type
    lsize = genprimsize(ltype);
    rsize = genprimsize(rtype);

    // Check if tree's size is too big
    if (lsize > rsize) return NULL;

    // Widen to the right
    if (rsize > lsize) return mkastunary(A_WIDEN, rtype, tree, 0);
  }

  // For pointers on the left
  if (ptrtype(ltype)) {
    // If it matches the rtype and we are not doing
    // a binary OP, we do not have to make any changes.
    if (op == 0 && ltype == rtype)
      return tree;
  }

  // We can scale on A_ADD or A_SUBTRACT operations
  if (op == A_ADD || op == A_SUBTRACT) {
    // Left is int type and right is ptr type
    if (inttype(ltype) && ptrtype(rtype)) {
      rsize = genprimsize(value_at(rtype));
      // Scale the int if necessary
      if (rsize > 1)
        return mkastunary(A_SCALE, rtype, tree, rsize);
      else
        return tree;
    }
  }

  return NULL;
}
