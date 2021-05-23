// Generic assembly code generator

#include "cg.h"
#include "data.h"
#include "gen.h"

// Given an AST node, recursively generate assembly
// code for it. Returns the identifier of the register
// that contains the results of evaluating this node.
//
// reg - Since we evaluate the left child before the right
//       child, this allows us to pass results from the evaluation
//       of the left child to aid the evaluation of the right child.
int genAST(struct ASTnode *n, int reg) {
  // Registers containing the results of evaluating
  // the left and right child nodes
  int leftreg, rightreg;

  if (n->left) leftreg = genAST(n->left, -1);
  if (n->right) rightreg = genAST(n->right, leftreg);

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
      return cgloadint(n->v.intvalue);
    case A_IDENT:
      return cgloadglob(Gsym[n->v.id].name);
    case A_LVIDENT:
      return cgstorglob(reg, Gsym[n->v.id].name);
    case A_ASSIGN:
      // Assignation should have been completed at the
      // A_LVIDENT node
      return rightreg;
    case A_EQ:
      return cgequal(leftreg, rightreg);
    case A_NE:
      return cgnotequal(leftreg, rightreg);
    case A_LT:
      return cglessthan(leftreg, rightreg);
    case A_GT:
      return cggreaterthan(leftreg, rightreg);
    case A_LE:
      return cglessequal(leftreg, rightreg);
    case A_GE:
      return cggreaterequal(leftreg, rightreg);
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}

void genpreamble()        { cgpreamble(); }
void genpostamble()       { cgpostamble(); }
void genfreeregs()        { freeall_registers(); }
void genprintint(int reg) { cgprintint(reg); }

void generatecode(struct ASTnode *n) {
  int reg;

  // Handle preamble (leading code)
  cgpreamble();
  reg = genAST(n, -1);
  // Print final result
  cgprintint(reg);

  // Handle postamble (trailing code)
  cgpostamble();
}

void genglobsym(char *s) { cgglobsym(s); }
