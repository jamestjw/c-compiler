// Generic assembly code generator

#include "cg.h"
#include "data.h"
#include "gen.h"
#include "misc.h"

// Generate and return a new label number
// each time this function is invoked.
int genlabel(void) {
  static int id = 1;
  return id++;
}

// Generate code for an IF statement and an
// optional ELSE clause
static int genIFAST(struct ASTnode *n) {
  int Lfalse, Lend;

  // Generate two labels, one for the false
  // compound statement, and another one for
  // the end of the overall if statement.
  // When there is no ELSE clause, Lfalse shall
  // be the ending label.
  Lfalse = genlabel();
  if (n->right)
    Lend = genlabel();

  // Generate the condition code followed by a
  // jump to the false label if the condition
  // evaluates to 0
  // Note: We cheat by passing the label as a register
  genAST(n->left, Lfalse, n->op);
  genfreeregs();

  // Generate the statement for the TRUE clause
  genAST(n->mid, NOREG, n->op);
  genfreeregs();

  // If there exists an ELSE clause, we skip it
  // by jumping to the end
  if (n->right)
    cgjump(Lend);

  cglabel(Lfalse);

  // Optional ELSE:
  // Generate the false statement and the end label
  if (n->right) {
    genAST(n->right, NOREG, n->op);
    genfreeregs();
    cglabel(Lend);
  }

  return NOREG;
}

static int genWHILE(struct ASTnode *n) {
  int Lstart, Lend;

  // Generate the start and end labels
  // and output the start label
  Lstart = genlabel();
  Lend = genlabel();
  cglabel(Lstart);

  // Generate the code for the conditional
  // followed by a jump to the end.
  genAST(n->left, Lend, n->op);
  genfreeregs();

  // Generate code for the body 
  genAST(n->right, NOREG, n->op);
  genfreeregs();

  // Output the jump to the beginning of the loop
  cgjump(Lstart);
  // Output the end label
  cglabel(Lend);

  return NOREG;
}

// Given an AST node, recursively generate assembly
// code for it. Returns the identifier of the register
// that contains the results of evaluating this node.
//
// reg - Since we evaluate the left child before the right
//       child, this allows us to pass results from the evaluation
//       of the left child to aid the evaluation of the right child.
// parentASTop - Operator of the parent AST node      
int genAST(struct ASTnode *n, int reg, int parentASTop) {
  // Registers containing the results of evaluating
  // the left and right child nodes
  int leftreg, rightreg;

  // Handle special cases
  switch (n->op) {
    case A_IF:
      return genIFAST(n);
    case A_WHILE:
      return genWHILE(n);
    case A_GLUE:
      genAST(n->left, NOREG, n->op);
      genfreeregs();
      genAST(n->right, NOREG, n->op);
      genfreeregs();
      return NOREG;
    case A_FUNCTION:
      // Generate the function preamble
      cgfuncpreamble(Gsym[n->v.id].name);
      genAST(n->left, NOREG, n->op);
      cgfuncpostamble(n->v.id);
      return NOREG;
  }

  if (n->left) leftreg = genAST(n->left, NOREG, n->op);
  if (n->right) rightreg = genAST(n->right, leftreg, n->op);

  switch (n->op) {
    case A_ADD:
      return cgadd(leftreg, rightreg);
    case A_SUBTRACT:
      return cgsub(leftreg, rightreg);
    case A_MULTIPLY:
      return cgmul(leftreg, rightreg);
    case A_DIVIDE:
      return cgdiv(leftreg, rightreg);
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:
      // If the parent node is an A_IF, generate a 
      // compare and jump, otherwise compare registers
      // and set to either 0 or 1
      if (parentASTop == A_IF || parentASTop == A_WHILE)
        return cgcompare_and_jump(n->op, leftreg, rightreg, reg);
      else
        return cgcompare_and_set(n->op, leftreg, rightreg);
    case A_INTLIT:
      return cgloadint(n->v.intvalue, n->type);
    case A_IDENT:
      // Load the value if it is an r-value
      // or if it is a dereference.
      if (n->rvalue || parentASTop == A_DEREF)
        return cgloadglob(n->v.id);
      else
        return NOREG;
    case A_ASSIGN:
      // Handle differently based on whether we are storing
      // to an identifier or through a pointer
      switch (n->right->op) {
        case A_IDENT:
          return cgstorglob(leftreg, n->right->v.id);
        case A_DEREF:
          // rightreg should contain the pointer to the
          // identifier to store to.
          return cgstorderef(leftreg, rightreg, n->right->type);
        default:
          fatald("Can't assign in genAST for op", n->op);
          break;
      }
    case A_WIDEN:
      return cgwiden(leftreg, n->left->type, n->type);
    case A_RETURN:
      cgreturn(leftreg, Functionid);
      return NOREG;
    case A_FUNCCALL:
      return cgcall(leftreg, n->v.id);
    case A_ADDR:
      return cgaddress(n->v.id);
    case A_DEREF:
      // If it is a r-value, dereference and load the value
      // into a register and return it
      if (n->rvalue)
        return cgderef(leftreg, n->left->type);
      else
        // Else return the pointer to be used as an lvalue
        return leftreg;
    case A_SCALE:
      switch (n->v.size) {
        // Use bitshifts for powers of 2
        case 2:
          return cgshlconst(leftreg, 1);
        case 4:
          return cgshlconst(leftreg, 2);
        case 8:
          return cgshlconst(leftreg, 3);
        default:
          rightreg = cgloadint(n->v.size, P_INT);
          return cgmul(leftreg, rightreg);
      }
    case A_STRLIT:
      return cgloadglobstr(n->v.id);
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}

void genpreamble()        { cgpreamble(); }
void genpostamble()       { cgpostamble(); }
void genfreeregs()        { freeall_registers(); }
void genprintint(int reg) { cgprintint(reg); }

void genglobsym(int id) { cgglobsym(id); }

int genprimsize(int type) {
  return cgprimsize(type);
}

int genglobstr(char *strvalue) {
  int l = genlabel();
  cgglobstr(l, strvalue);
  return l;
}
