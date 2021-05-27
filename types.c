#include "defs.h"
#include "gen.h"

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
