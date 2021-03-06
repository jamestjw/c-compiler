#include "defs.h"
#include "gen.h"
#include "misc.h"
#include "tree.h"
#include "types.h"

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
  // Check if this will exceed the max level
  // of indirection
  if ((type & 0xf) == 0xf)
    fatald("Unrecognised in pointer_to: type", type);

  return (type + 1);
}

int value_at(int type) {
  // Check that this is a pointer
  if ((type & 0xf) == 0x0)
    fatald("Unrecognised in value_at: type", type);

  return (type - 1);
}

int inttype(int type) {
  // Check that this is not a pointer
  // and it is indeed an int type
  return (((type & 0xf) == 0) && (type >= P_CHAR && type <= P_LONG));
}

// Return true if the type is of a pointer type
int ptrtype(int type) {
  // Check that this is a pointer
  return ((type & 0xf) != 0);
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
struct ASTnode *modify_type(struct ASTnode *tree, int rtype,
        struct symtable *rctype, int op) {
  int ltype;
  int lsize, rsize;

  ltype = tree->type;

  // For A_LOGOR and A_LOGAND, both types have to be int or pointer types
  if (op == A_LOGOR || op == A_LOGAND) {
    if (!inttype(ltype) && !ptrtype(ltype))
      return(NULL);
    if (!inttype(ltype) && !ptrtype(rtype))
      return(NULL);
    return (tree);
  }

  if (ltype == P_STRUCT || ltype == P_UNION)
    fatal("Don't know how to do this yet");
  if (rtype == P_STRUCT || rtype == P_UNION)
    fatal("Don't know how to do this yet");

  // Handle integer types
  if (inttype(ltype) && inttype(rtype)) {
    // Check if types already match
    if (ltype == rtype) return tree;

    // Get the sizes of each type
    lsize = typesize(ltype, NULL);
    rsize = typesize(rtype, NULL);

    // Check if tree's size is too big
    if (lsize > rsize) return NULL;

    // Widen to the right
    if (rsize > lsize) return mkastunary(A_WIDEN, rtype, NULL, tree, NULL, 0);
  }

  // For pointers
  if (ptrtype(ltype) && ptrtype(rtype)) {
    if (op >= A_EQ && op <= A_GE)
      return tree;

    // Comparison of the same type for non-binary operations is fine (
    // like assignments), or when the left type is of 'void *' type
    if (op == 0 && (ltype == rtype || ltype == pointer_to(P_VOID)))
      return tree;
  }

  // We can scale on A_ADD or A_SUBTRACT operations
  if (op == A_ADD || op == A_SUBTRACT ||
      op == A_ASPLUS || op == A_ASMINUS) {
    // Left is int type and right is ptr type
    if (inttype(ltype) && ptrtype(rtype)) {
      rsize = genprimsize(value_at(rtype));
      // Scale the int if necessary
      if (rsize > 1)
        return mkastunary(A_SCALE, rtype, rctype, tree, NULL, rsize);
      else
        return tree;
    }
  }

  return NULL;
}

int typesize(int type, struct symtable *ctype) {
  if (type == P_STRUCT || type == P_UNION)
    return ctype->size;

  return genprimsize(type);
}
