// Generic assembly code generator

#include "data.h"
#include "cg.h"
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

// Generate code to copy the arguments of a
// function call to the right places, then
// invoke the function itself. Return the
// register that holds the function's return
// value.
static int gen_funccall(struct ASTnode *n) {
  struct ASTnode *gluetree = n->left;
  int reg;
  int numargs = 0;

  while (gluetree) {
    reg = genAST(gluetree->right, NOLABEL, gluetree->op);
    // The size param indicates that this the nth argument
    // to be passed to the function
    cgcopyarg(reg, gluetree->size);

    // Note down the total number of arguments (first gluetree
    // we encounter is the one with the biggest count)
    if (numargs == 0)
      numargs = gluetree->size;

    genfreeregs();
    gluetree = gluetree->left;
  }

  return cgcall(n->sym, numargs);
}

// Given an AST node, recursively generate assembly
// code for it. Returns the identifier of the register
// that contains the results of evaluating this node.
//
// reg - Since we evaluate the left child before the right
//       child, this allows us to pass results from the evaluation
//       of the left child to aid the evaluation of the right child.
// parentASTop - Operator of the parent AST node      
int genAST(struct ASTnode *n, int label, int parentASTop) {
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
      cgfuncpreamble(n->sym);
      genAST(n->left, NOREG, n->op);
      cgfuncpostamble(n->sym);
      return NOREG;
    case A_FUNCCALL:
      return gen_funccall(n);
  }

  if (n->left) leftreg = genAST(n->left, NOREG, n->op);
  if (n->right) rightreg = genAST(n->right, NOREG, n->op);

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
        return cgcompare_and_jump(n->op, leftreg, rightreg, label);
      else
        return cgcompare_and_set(n->op, leftreg, rightreg);
    case A_INTLIT:
      return cgloadint(n->intvalue, n->type);
    case A_IDENT:
      // Load the value if it is an r-value
      // or if it is a dereference.
      if (n->rvalue || parentASTop == A_DEREF) {
        if (n->sym->class == C_GLOBAL) {
          return cgloadglob(n->sym, n->op);
        } else {
          return cgloadlocal(n->sym, n->op);
        }
      } else {
        return NOREG;
      }
    case A_ASSIGN:
      // Handle differently based on whether we are storing
      // to an identifier or through a pointer
      switch (n->right->op) {
        case A_IDENT:
          if (n->right->sym->class == C_GLOBAL)
            return cgstorglob(leftreg, n->right->sym);
          else
            return cgstorlocal(leftreg, n->right->sym);
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
    case A_ADDR:
      return cgaddress(n->sym);
    case A_DEREF:
      // If it is a r-value, dereference and load the value
      // into a register and return it
      if (n->rvalue)
        return cgderef(leftreg, n->left->type);
      else
        // Else return the pointer to be used as an lvalue
        return leftreg;
    case A_SCALE:
      switch (n->size) {
        // Use bitshifts for powers of 2
        case 2:
          return cgshlconst(leftreg, 1);
        case 4:
          return cgshlconst(leftreg, 2);
        case 8:
          return cgshlconst(leftreg, 3);
        default:
          rightreg = cgloadint(n->size, P_INT);
          return cgmul(leftreg, rightreg);
      }
    case A_STRLIT:
      return cgloadglobstr(n->intvalue);
    case A_AND:
      return cgand(leftreg, rightreg);
    case A_OR:
      return cgor(leftreg, rightreg);
    case A_XOR:
      return cgxor(leftreg, rightreg);
    case A_LSHIFT:
      return cgshl(leftreg, rightreg);
    case A_RSHIFT:
      return cgshr(leftreg, rightreg);
    case A_POSTINC:
    case A_POSTDEC:
      if (n->sym->class == C_GLOBAL)
        return (cgloadglob(n->sym, n->op));
      else
        return (cgloadlocal(n->sym, n->op));
    case A_PREINC:
    case A_PREDEC:
      if (n->left->sym->class == C_GLOBAL)
        return (cgloadglob(n->left->sym, n->op));
      else
        return (cgloadlocal(n->left->sym, n->op));
    case A_NEGATE:
      return cgnegate(leftreg);
    case A_INVERT:
      return cginvert(leftreg);
    case A_LOGNOT:
      return cglognot(leftreg);
    case A_TOBOOL:
      return cgboolean(leftreg, parentASTop, label);
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }
}

void genpreamble()        { cgpreamble(); }
void genpostamble()       { cgpostamble(); }
void genfreeregs()        { freeall_registers(); }
void genprintint(int reg) { cgprintint(reg); }

void genglobsym(struct symtable *node) { cgglobsym(node); }

int genprimsize(int type) {
  return cgprimsize(type);
}

int genglobstr(char *strvalue) {
  int l = genlabel();
  cgglobstr(l, strvalue);
  return l;
};

