// Generic assembly code generator

#include "cg.h"
#include "defs.h"
#include "gen.h"

// Given an AST node, recursively generate assembly
// code for it. Returns the identifier of the register
// that contains the results of evaluating this node.
static int genAST(struct ASTnode *n) {
  // Registers containing the results of evaluating
  // the left and right child nodes
  int leftreg, rightreg;

  if (n->left) leftreg = genAST(n->left);
  if (n->right) rightreg = genAST(n->right);

  switch (n->op) {
    case A_ADD:
      return cgadd(leftreg, rightreg);
    case A_SUBTRACT:
      return cgsub(leftreg, rightreg);
    case A_MULTIPLY:
      return cgmul(leftreg, rightreg);
    case A_DIVIDE:
      return cgdiv(leftreg, rightreg);
    case A_INTLIT:
      return cgload(n->intvalue);
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}

void generatecode(struct ASTnode *n) {
  int reg;

  // Handle preamble (leading code)
  cgpreamble();
  reg = genAST(n);
  // Print final result
  cgprintint(reg);

  // Handle postamble (trailing code)
  cgpostamble();
}
