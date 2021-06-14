#include "defs.h"
#include "interp.h"

#ifdef DEBUG_INTERP
// List of AST operators
static char *ASTop[] = { "+", "-", "*", "/" };
#endif

int interpretAST(struct ASTnode *n) {
  int leftval, rightval;

  if (n->left)
    leftval = interpretAST(n->left);

  if (n->right)
    rightval = interpretAST(n->right);

  #ifdef DEBUG_INTERP
    if (n->op == A_INTLIT)
      printf("int %d\n", n->a_intvalue);
    else
      printf("%d %s %d\n", leftval, ASTop[n->op], rightval);
  #endif

  switch(n->op) {
    case A_ADD:
      return (leftval + rightval);
    case A_SUBTRACT:
      return (leftval - rightval);
    case A_MULTIPLY:
      return (leftval * rightval);
    case A_DIVIDE:
      return (leftval / rightval);
    case A_INTLIT:
      return n->a_intvalue;
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}
