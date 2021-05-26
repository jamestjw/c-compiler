#include "defs.h"

// Given two primitive types, return true if they are compatible.
// Sets *left and *right to either 0 or A_WIDEN depending on whether
// one has to be widened to match the other.
//
// onlyright - Only widen left to right.
int type_compatible(int *left, int *right, int onlyright) {
  // Voids are never compatible with anything
  if ((*left == P_VOID) || (*right == P_VOID)) return 0;


  // Same types are compatible
  if (*left == *right) { *left = *right = 0; return 1;  }

  // Widen P_CHARs to P_INTs
  if ((*left == P_CHAR) && (*right == P_INT)) {
    *left = A_WIDEN; *right = 0; return 1;
  }

  if ((*left == P_INT) && (*right == P_CHAR)) {
    // Since we are not allowed to widen the value on the right
    if (onlyright) return 0;
    *left = 0; *right = A_WIDEN; return 1;
  }

  // Anything else is compatible
  *left = *right = 0;
  return 1;
}
