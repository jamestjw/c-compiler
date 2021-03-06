// Generic assembly code generator

#include "data.h"
#include "cg.h"
#include "gen.h"
#include "misc.h"

static int labelid = 1;

// Generate and return a new label number
// each time this function is invoked.
int genlabel(void) {
  return labelid++;
}

// Generate code for an IF statement and an
// optional ELSE clause
static int genIF(struct ASTnode *n, int looptoplabel, int loopendlabel) {
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
  genAST(n->left, Lfalse, NOLABEL, NOLABEL, n->op);
  genfreeregs(NOREG);

  // Generate the statement for the TRUE clause
  genAST(n->mid, NOLABEL, looptoplabel, loopendlabel, n->op);
  genfreeregs(NOREG);

  // If there exists an ELSE clause, we skip it
  // by jumping to the end
  if (n->right)
    cgjump(Lend);

  cglabel(Lfalse);

  // Optional ELSE:
  // Generate the false statement and the end label
  if (n->right) {
    genAST(n->right, NOLABEL, NOLABEL, loopendlabel, n->op);
    genfreeregs(NOREG);
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
  genAST(n->left, Lend, Lstart, Lend, n->op);
  genfreeregs(NOREG);

  // Generate code for the body
  genAST(n->right, NOLABEL, Lstart, Lend, n->op);
  genfreeregs(NOREG);

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

  // Save registers before we copy the arguments
  spill_all_regs();

  while (gluetree) {
    reg = genAST(gluetree->right, NOLABEL, NOLABEL, NOLABEL, gluetree->op);
    // The size param indicates that this the nth argument
    // to be passed to the function
    cgcopyarg(reg, gluetree->a_size);

    // Note down the total number of arguments (first gluetree
    // we encounter is the one with the biggest count)
    if (numargs == 0)
      numargs = gluetree->a_size;

    gluetree = gluetree->left;
  }

  return cgcall(n->sym, numargs);
}

static int genSWITCH(struct ASTnode *n) {
  int *caseval, *caselabel;
  int Ljumptop, Lend;
  int i, reg, defaultlabel = 0, casecount = 0;
  struct ASTnode *c;

  // Create arrays for case values and their corresponding
  // labels.
  caseval = (int *) malloc((n->a_intvalue + 1) * sizeof(int));
  caselabel = (int *) malloc((n->a_intvalue + 1) * sizeof(int));

  Ljumptop = genlabel();
  Lend = genlabel();
  // Set default label to Lend for now,
  // we will check later if a default
  // case is available.
  defaultlabel = Lend;

  reg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, 0);
  cgjump(Ljumptop);
  genfreeregs(reg);

  for (i = 0, c = n->right; c != NULL; i++, c = c->right) {
    caselabel[i] = genlabel();
    caseval[i] = c->a_intvalue;
    cglabel(caselabel[i]);
    if (c->op == A_DEFAULT)
      // Update default label with the right label
      defaultlabel = caselabel[i];
    else
      casecount++;

    // Passing in the end label to allow breaks
    if (c->left)
      genAST(c->left, NOLABEL, NOLABEL, Lend, 0);
    genfreeregs(NOREG);
  }

  // Include a jump to the end of the switch after the last
  // branch.
  cgjump(Lend);

  cgswitch(reg, casecount, Ljumptop, caselabel, caseval, defaultlabel);

  cglabel(Lend);

  return NOREG;
}

static int gen_ternary(struct ASTnode *n) {
  int Lfalse, Lend;
  int reg, expreg;

  // Generate labels for the false expression
  // and the end of the overall expression.
  Lfalse = genlabel();
  Lend = genlabel();

  // Generate condition code followed by jump to
  // the false label
  genAST(n->left, Lfalse, NOLABEL, NOLABEL, n->op);

  // Alloc register to store result of ternary expr
  reg = alloc_register();

  // Generate code for the true expression, and the false label
  expreg = genAST(n->mid, NOLABEL, NOLABEL, NOLABEL, n->op);
  // Move the expression result into the known register
  cgmove(expreg, reg);
  cgfreereg(expreg);
  cgjump(Lend);
  cglabel(Lfalse);

  // Generate the false expression and the end label
  expreg = genAST(n->right, NOLABEL, NOLABEL, NOLABEL, n->op);
  // Move expression result into the known register
  cgmove(expreg, reg);
  // Do not free register containing result
  cgfreereg(expreg);
  cglabel(Lend);

  return reg;
}

static int gen_logandor(struct ASTnode *n) {
  int Lfalse = genlabel();
  int Lend = genlabel();
  int reg;

  // Generate code for the left expression, followed
  // by a jump to the Lfalse label
  reg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, 0);
  cgboolean(reg, n->op, Lfalse);
  genfreeregs(NOREG);

  reg = genAST(n->right, NOLABEL, NOLABEL, NOLABEL, 0);
  cgboolean(reg, n->op, Lfalse);
  genfreeregs(reg);

  // The code below executes when there are no jumps
  if (n->op == A_LOGAND) {
    // If we get here for LOGAND, it must means that
    // both expressions are true.
    cgloadboolean(reg, 1);
    cgjump(Lend);
    cglabel(Lfalse);
    cgloadboolean(reg, 0);
  } else {
    cgloadboolean(reg, 0);
    cgjump(Lend);
    cglabel(Lfalse);
    // For logand, we set to true after jumping
    cgloadboolean(reg, 1);
  }
  cglabel(Lend);
  return reg;
}

// Given an AST node, recursively generate assembly
// code for it. Returns the identifier of the register
// that contains the results of evaluating this node.
//
// parentASTop - Operator of the parent AST node
int genAST(struct ASTnode *n, int iflabel,
    int looptoplabel, int loopendlabel, int parentASTop) {
  // Registers containing the results of evaluating
  // the left and right child nodes
  int leftreg=NOREG, rightreg=NOREG;

  if (n == NULL) return NOREG;

  // Handle special cases
  switch (n->op) {
    case A_IF:
      return genIF(n, looptoplabel, loopendlabel);
    case A_WHILE:
      return genWHILE(n);
    case A_GLUE:
      if (n->left != NULL) genAST(n->left, iflabel, looptoplabel, loopendlabel, n->op);
      genfreeregs(NOREG);
      if (n->right != NULL) genAST(n->right, iflabel, looptoplabel, loopendlabel, n->op);
      genfreeregs(NOREG);
      return NOREG;
    case A_FUNCTION:
      // Generate the function preamble
      cgfuncpreamble(n->sym);
      genAST(n->left, NOREG, NOREG, NOREG, n->op);
      cgfuncpostamble(n->sym);
      return NOREG;
    case A_FUNCCALL:
      return gen_funccall(n);
    case A_SWITCH:
      return genSWITCH(n);
    case A_TERNARY:
      return gen_ternary(n);
    case A_LOGOR:
      return gen_logandor(n);
    case A_LOGAND:
      return gen_logandor(n);
  }

  if (n->left) leftreg = genAST(n->left, NOLABEL, NOLABEL, NOLABEL, n->op);
  if (n->right) rightreg = genAST(n->right, NOLABEL, NOLABEL, NOLABEL, n->op);

  switch (n->op) {
    case A_ADD:
      return cgadd(leftreg, rightreg);
    case A_SUBTRACT:
      return cgsub(leftreg, rightreg);
    case A_MULTIPLY:
      return cgmul(leftreg, rightreg);
    case A_DIVIDE:
      return cgdivmod(leftreg, rightreg, A_DIVIDE);
    case A_MOD:
      return (cgdivmod(leftreg, rightreg, A_MOD));
    case A_EQ:
    case A_NE:
    case A_LT:
    case A_GT:
    case A_LE:
    case A_GE:
      // If the parent node is an A_IF, generate a
      // compare and jump, otherwise compare registers
      // and set to either 0 or 1
      if (parentASTop == A_IF || parentASTop == A_WHILE || parentASTop == A_TERNARY)
        return cgcompare_and_jump(n->op, leftreg, rightreg, iflabel, n->left->type);
      else
        return cgcompare_and_set(n->op, leftreg, rightreg, n->left->type);
    case A_INTLIT:
      return cgloadint(n->a_intvalue, n->type);
    case A_IDENT:
      // Load the value if it is an r-value
      // or if it is a dereference.
      if (n->rvalue || parentASTop == A_DEREF) {
        return cgloadvar(n->sym, n->op);
      } else {
        return NOREG;
      }
    case A_ASSIGN:
    case A_ASPLUS:
    case A_ASMINUS:
    case A_ASSTAR:
    case A_ASSLASH:
    case A_ASMOD:
      // For '+=' and friends, generate suitable code and
      // get the register with the result. Then take the
      // left child and make it the right child so that
      // we can fall into the assignment code.
      switch(n->op) {
        case A_ASPLUS:
          leftreg = cgadd(leftreg, rightreg);
          n->right = n->left;
          break;
        case A_ASMINUS:
          leftreg = cgsub(leftreg, rightreg);
          n->right = n->left;
          break;
        case A_ASSTAR:
          leftreg = cgmul(leftreg, rightreg);
          n->right = n->left;
          break;
        case A_ASSLASH:
          leftreg = cgdivmod(leftreg, rightreg, A_DIVIDE);
          n->right = n->left;
          break;
        case A_ASMOD:
          leftreg = cgdivmod(leftreg, rightreg, A_MOD);
          n->right = n->left;
          break;
      }

      // Handle differently based on whether we are storing
      // to an identifier or through a pointer
      switch (n->right->op) {
        case A_IDENT:
          if (n->right->sym->class == C_GLOBAL ||
              n->right->sym->class == C_STATIC ||
              n->right->sym->class == C_EXTERN )
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
      // If we have a symbol, get its address. Otherwise
      // the left register already has the address because
      // it's a chained member access.
      if (n->sym != NULL)
        return cgaddress(n->sym);
      else
        return leftreg;
    case A_DEREF:
      // If it is a r-value, dereference and load the value
      // into a register and return it
      if (n->rvalue)
        return cgderef(leftreg, n->left->type);
      else
        // Else return the pointer to be used as an lvalue
        return leftreg;
    case A_SCALE:
      switch (n->a_size) {
        // Use bitshifts for powers of 2
        case 2:
          return cgshlconst(leftreg, 1);
        case 4:
          return cgshlconst(leftreg, 2);
        case 8:
          return cgshlconst(leftreg, 3);
        default:
          rightreg = cgloadint(n->a_size, P_INT);
          return cgmul(leftreg, rightreg);
      }
    case A_STRLIT:
      return cgloadglobstr(n->a_intvalue);
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
      return cgloadvar(n->sym, n->op);
    case A_PREINC:
    case A_PREDEC:
      return cgloadvar(n->left->sym, n->op);
    case A_NEGATE:
      return cgnegate(leftreg);
    case A_INVERT:
      return cginvert(leftreg);
    case A_LOGNOT:
      return cglognot(leftreg);
    case A_TOBOOL:
      return cgboolean(leftreg, parentASTop, iflabel);
    case A_BREAK:
      cgjump(loopendlabel);
      return NOREG;
    case A_CONTINUE:
      cgjump(looptoplabel);
      return NOREG;
    case A_CAST:
      return leftreg; // Not much to do
    default:
      fprintf(stderr, "Unknown AST operator %d\n", n->op);
      exit(1);
  }

  // -Wall
  return NOREG;
}

void genpreamble()        { cgpreamble(); }
void genpostamble()       { cgpostamble(); }
void genfreeregs(int keepreg)        { freeall_registers(keepreg); }
void genprintint(int reg) { cgprintint(reg); }

void genglobsym(struct symtable *node) { cgglobsym(node); }

int genprimsize(int type) {
  return cgprimsize(type);
}

int genglobstr(char *strvalue, int append) {
  int l = genlabel();
  cgglobstr(l, strvalue, append);
  return l;
};

void genglobstrend(void) {
  cgglobstrend();
}

int genalign(int type, int offset, int direction) {
  return cgalign(type, offset, direction);
}
