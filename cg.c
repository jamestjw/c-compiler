#include <stdio.h>
#include <stdlib.h>
#include "data.h"
#include "cg.h"
#include "gen.h"
#include "misc.h"
#include "types.h"

// Flag to say which section were are outputting in to
enum {
    no_seg, text_seg, data_seg
} currSeg = no_seg;

#define NUMFREEREGS 4
#define FIRSTPARAMREG 9   // Position of first parameter register

// List of generic registers that the code will work on
static char *reglist[] =
        {"%r10", "%r11", "%r12", "%r13", "%r9", "%r8", "%rcx", "%rdx", "%rsi",
         "%rdi"};
// Lower 8 bits of registers in reglist
static char *breglist[] =
        {"%r10b", "%r11b", "%r12b", "%r13b", "%r9b", "%r8b", "%cl", "%dl", "%sil",
         "%dil"};
// Lower 4 bytes of registers in reglist
static char *dreglist[] =
        {"%r10d", "%r11d", "%r12d", "%r13d", "%r9d", "%r8d", "%ecx", "%edx",
         "%esi", "%edi"};

// List of available registers
static int freereg[NUMFREEREGS];

// List of comparison instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] = {"sete", "setne", "setl", "setg", "setle", "setge"};

// List of inverted jump instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = {"jne", "je", "jge", "jle", "jg", "jl"};

// Position of next local variable relative to stack base pointer
static int localOffset;
static int stackOffset;

// Next register to spill on the stack
// When this value is 0, it means that
// we have no spilled registers.
static int spillreg = 0;

void cgtextseg() {
  if (currSeg != text_seg) {
    fputs("\t.text\n", Outfile);
    currSeg = text_seg;
  }
}

void cgdataseg() {
  if (currSeg != data_seg) {
    fputs("\t.data\n", Outfile);
    currSeg = data_seg;
  }
}

// Mark all registers as available
// If keepreg is positive, don't free that one
void freeall_registers(int keepreg) {
  int i;
  for (i = 0; i < NUMFREEREGS; i++) {
    if (i != keepreg)
      freereg[i] = 1;
  }
}

// Push a register to the stack
static void pushreg(int r) {
  fprintf(Outfile, "\tpushq\t%s\n", reglist[r]);
}

// Pop a register off the stack
static void popreg(int r) {
  fprintf(Outfile, "\tpopq\t%s\n", reglist[r]);
}

// Allocate a free register and returns the corresponding
// identifier. Program exits if no available registers remain.
int alloc_register(void) {
  int reg;

  for (reg = 0; reg < NUMFREEREGS; reg++) {
    if (freereg[reg]) {
      freereg[reg] = 0;
      return reg;
    }
  }

  // Since we've no available registers, we spill one
  reg = (spillreg % NUMFREEREGS);
  spillreg++;
  pushreg(reg);
  return reg;
}

// Frees up a previously allocated register.
void cgfreereg(int reg) {
  if (freereg[reg] != 0) {
    fatald("Error trying to free register", reg);
  }

  // If there was a spilled register
  // Note: We always reload the most recently-spilled register
  if (spillreg > 0) {
    spillreg--;
    reg = (spillreg % NUMFREEREGS);
    popreg(reg);
  } else {
    freereg[reg] = 1;
  }
}

// Spill all registers on the stack
void spill_all_regs(void) {
  int i;

  for (i = 0; i < NUMFREEREGS; i++) {
    pushreg(i);
  }
}

static void unspill_all_regs(void) {
  int i;

  for (i = NUMFREEREGS - 1; i >= 0; i--)
    popreg(i);
}

static int newlocaloffset(int size) {
  // Decrement by a minimum of 4 bytes
  localOffset += (size > 4) ? size : 4;
  return -localOffset;
}

void cgpreamble() {
  freeall_registers(NOREG);
  cgtextseg();
  fprintf(Outfile,
          "# internal switch(expr) routine\n"
          "# %%rsi = switch table, %%rax = expr\n"
          "# from SubC: http://www.t3x.org/subc/\n"
          "\n"
          "__switch:\n"
          "        pushq   %%rsi\n"
          "        movq    %%rdx, %%rsi\n"
          "        movq    %%rax, %%rbx\n"
          "        cld\n"
          "        lodsq\n"
          "        movq    %%rax, %%rcx\n"
          "__next:\n"
          "        lodsq\n"
          "        movq    %%rax, %%rdx\n"
          "        lodsq\n"
          "        cmpq    %%rdx, %%rbx\n"
          "        jnz     __no\n"
          "        popq    %%rsi\n"
          "        jmp     *%%rax\n"
          "__no:\n"
          "        loop    __next\n"
          "        lodsq\n"
          "        popq    %%rsi\n"
          "        jmp     *%%rax\n"
          "\n");
}

void cgfuncpreamble(struct symtable *sym) {
  char *name = sym->name;
  struct symtable *parm, *locvar;
  int cnt;
  int paramOffset = 16;           // Offset of first param (relative to %rbp)
  int paramReg = FIRSTPARAMREG;   // Index to first param register

  // Generate output in text segment
  cgtextseg();
  // Reset offset
  localOffset = 0;

  // Output the function start, save the %rsp and %rsp
  if (sym->class == C_GLOBAL)
    fprintf(Outfile, "\t.globl\t%s\n"
                     "\t.type\t%s, @function\n", name, name);

  fprintf(Outfile,
          "%s:\n" "\tpushq\t%%rbp\n"
          "\tmovq\t%%rsp, %%rbp\n", name);

  // Copy in-register parameters to the stack
  for (parm = sym->member, cnt = 1; parm != NULL; parm = parm->next, cnt++) {
    // Only 6 params are passed in registers
    if (cnt > 6) {
      parm->st_posn = paramOffset;
      paramOffset += 8;
    } else {
      parm->st_posn = newlocaloffset(parm->size);
      cgstorlocal(paramReg--, parm);
    }
  }

  // For locals, we shall create a new
  // stack position.
  for (locvar = Loclhead; locvar != NULL; locvar = locvar->next) {
    locvar->st_posn = newlocaloffset(locvar->size);
  }

  // Align stack pointer to be a multiple of 16
  stackOffset = (localOffset + 15) & ~15;
  // Decrement stack pointer based on how many
  // variables we loaded onto the stack
  fprintf(Outfile, "\taddq\t$%d, %%rsp\n", -stackOffset);
}

void cgfuncpostamble(struct symtable *sym) {
  cglabel(sym->st_endlabel);
  // Restore stack pointer
  fprintf(Outfile, "\taddq\t$%d, %%rsp\n", stackOffset);
  fputs(
          "\tpopq %rbp\n"
          "\tret\n",
          Outfile
  );
  freeall_registers(NOREG);
}

void cgpostamble() {}

// Allocates a register and loads an integer literal into it.
// For x86-64, we ignore the type
int cgloadint(int value, int type) {
  int r = alloc_register();

  // e.g. movq 10, %r10
  fprintf(Outfile, "\tmovq\t$%d, %s\n", value, reglist[r]);

  return r;
}

// Adds two registers and saves the result in one of them
// while freeing the other.
int cgadd(int r1, int r2) {
  // e.g. addq %r8, %r9
  fprintf(Outfile, "\taddq\t%s, %s\n", reglist[r2], reglist[r1]);
  cgfreereg(r2);

  return r1;
}

// Multiplies two registers and saves the result in one of them
// while freeing the other.
int cgmul(int r1, int r2) {
  // e.g. imulq %r8, %r9
  fprintf(Outfile, "\timulq\t%s, %s\n", reglist[r2], reglist[r1]);
  cgfreereg(r2);

  return r1;
}

// Subtracts the second register from the first and returns
// the register with the result while freeing the other.
int cgsub(int r1, int r2) {
  // e.g. subq %r2, %r1
  fprintf(Outfile, "\tsubq\t%s, %s\n", reglist[r2], reglist[r1]);
  cgfreereg(r2);

  return r1;
}

// Divide first register by the second and returns the
// register containing the result while freeing the other.
int cgdivmod(int r1, int r2, int op) {
  // Move dividend to %rax
  // e.g. movq %r1, %rax
  fprintf(Outfile, "\tmovq\t%s, %%rax\n", reglist[r1]);

  // Extend dividend to 8 bytes
  // e.g. cqo
  fprintf(Outfile, "\tcqo\n");

  // Divide the dividend in rax with the divisor in r2,
  // the resulting quotient will be in %rax
  // e.g. idivq %r2
  fprintf(Outfile, "\tidivq\t%s\n", reglist[r2]);

  if (op == A_DIVIDE)
    // Move result from %rax to %r1
    // e.g. movq %rax, %r1
    fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[r1]);
  else
    fprintf(Outfile, "\tmovq\t%%rdx,%s\n", reglist[r1]);

  cgfreereg(r2);

  return r1;
}

// Calls the printint function in the preamble to
// print an integer
void cgprintint(int r) {
  // Linux x86-64 expects the first argument to be in %rdi
  fprintf(Outfile, "\tmovq\t%s, %%rdi\n", reglist[r]);
  fprintf(Outfile, "\tcall\tprintint\n");
  cgfreereg(r);
}

int cgstorglob(int r, struct symtable *sym) {
  switch (cgprimsize(sym->type)) {
    case 1:
      // Only move a single byte for chars
      // e.g. movb %r10b, identifier(%rip)
      fprintf(Outfile, "\tmovb\t%s, %s(%%rip)\n", breglist[r], sym->name);
      break;
    case 4:
      // e.g. movl %r10d, identifier(%rip)
      fprintf(Outfile, "\tmovl\t%s, %s(%%rip)\n", dreglist[r], sym->name);
      break;
    case 8:
      // e.g. movq %r10, identifier(%rip)
      fprintf(Outfile, "\tmovq\t%s, %s(%%rip)\n", reglist[r], sym->name);
      break;
    default:
      fatald("Bad type in cgloadglob", sym->type);
  }

  return r;
}

void cgglobsym(struct symtable *sym) {
  int size, type;
  int initvalue;
  int i, j;

  if (sym == NULL)
    return;
  if (sym->stype == S_FUNCTION)
    return;

  if (sym->stype == S_ARRAY) {
    // Since arrays are pointers
    type = value_at(sym->type);
    size = typesize(type, sym->ctype);
  } else {
    size = sym->size;
    type = sym->type;
  }

  // .data
  // .globl varname
  // varname:   .long   0
  //            .long   0
  //            .long   0
  cgdataseg();
  if (sym->class == C_GLOBAL)
    fprintf(Outfile, "\t.globl\t%s\n", sym->name);
  fprintf(Outfile, "%s:", sym->name);

  for (i = 0; i < sym->nelems; i++) {
    initvalue = 0;
    if (sym->initlist != NULL)
      initvalue = sym->initlist[i];

    switch (size) {
      case 1:
        fprintf(Outfile, "\t.byte\t%d\n", initvalue);
        break;
      case 4:
        fprintf(Outfile, "\t.long\t%d\n", initvalue);
        break;
      case 8:
        // Generate ptr to string literal
        if (sym->initlist != NULL && type == pointer_to(P_CHAR) && initvalue != 0)
          fprintf(Outfile, "\t.quad\tL%d\n", initvalue);
        else
          fprintf(Outfile, "\t.quad\t%d\n", initvalue);
        break;
      default:
        for (j = 0; j < size; j++) {
          fprintf(Outfile, "\t.byte\t0\n");
        }
    }
  }
}

int cgcompare_and_set(int ASTop, int r1, int r2, int type) {
  int size = cgprimsize(type);

  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  switch (size) {
    // cmpq %r2, %r1
    // This calculates %r1 - %r2
    case 1:
      fprintf(Outfile, "\tcmpb\t%s, %s\n", breglist[r2], breglist[r1]);
      break;
    case 4:
      fprintf(Outfile, "\tcmpl\t%s, %s\n", dreglist[r2], dreglist[r1]);
      break;
    default:
      fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
  }

  // setge %r10b
  // This only sets the lowest byte of the register
  // Note: These instructions only works on 8-bit registers
  fprintf(Outfile, "\t%s\t%s\n", cmplist[ASTop - A_EQ], breglist[r2]);
  // movzbq %r10b, %r10
  // Moves the lowest bytes from one register and extends it to fill
  // a 64-bit register
  fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r2], reglist[r2]);

  cgfreereg(r1);
  return r2;
}

void cglabel(int l) {
  // L1:
  fprintf(Outfile, "L%d:\n", l);
}

void cgjump(int l) {
  // jmp L1
  fprintf(Outfile, "\tjmp\tL%d\n", l);
}

int cgcompare_and_jump(int ASTop, int r1, int r2, int label, int type) {
  int size = cgprimsize(type);

  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_jump()");

  switch (size) {
    // cmpq %r2, %r1
    case 1:
      fprintf(Outfile, "\tcmpb\t%s, %s\n", breglist[r2], breglist[r1]);
      break;
    case 4:
      fprintf(Outfile, "\tcmpl\t%s, %s\n", dreglist[r2], dreglist[r1]);
      break;
    default:
      fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
  }

  // jne L1
  fprintf(Outfile, "\t%s\tL%d\n", invcmplist[ASTop - A_EQ], label);
  cgfreereg(r1);
  cgfreereg(r2);
  return NOREG;
}

int cgwiden(int r, int oldtype, int newtype) {
  // cgloadglob has already taken care of the widening
  // of P_CHAR variables, hence we have nothing to do
  // here.
  return r;
}

int cgprimsize(int type) {
  if (ptrtype(type))
    return 8;

  switch (type) {
    case P_CHAR:
      return 1;
    case P_INT:
      return 4;
    case P_LONG:
      return 8;
    default:
      fatald("Bad type in cgprimsize", type);
  }

  // Handle -Wall violation
  return 0;
}

int cgcall(struct symtable *sym, int numargs) {
  int outr;

  // call funcname
  fprintf(Outfile, "\tcall\t%s@PLT\n", sym->name);

  // Remove arguments pushed to the stack
  if (numargs > 6)
    // addq $16, %rsp
    fprintf(Outfile, "\taddq\t$%d, %%rsp\n", 8 * (numargs - 6));

  unspill_all_regs();

  outr = alloc_register();
  // Move return code from %rax
  // movq %rax, %r9
  fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[outr]);

  return outr;
}

void cgreturn(int reg, struct symtable *sym) {
  // If there is a register containing the return value
  if (reg != NOREG) {
    if (ptrtype(sym->type))
      fprintf(Outfile, "\tmovq\t%s, %%rax\n", reglist[reg]);
    else {
      // Move return value to %rax
      switch (sym->type) {
        case P_CHAR:
          fprintf(Outfile, "\tmovzbl\t%s, %%eax\n", breglist[reg]);
          break;
        case P_INT:
          fprintf(Outfile, "\tmovl\t%s, %%eax\n", dreglist[reg]);
          break;
        case P_LONG:
          fprintf(Outfile, "\tmovq\t%s, %%rax\n", reglist[reg]);
          break;
        default:
          fatald("Bad function type in cgreturn", sym->type);
      }
    }
  }

  cgjump(sym->st_endlabel);
}

int cgaddress(struct symtable *sym) {
  int r = alloc_register();

  if (sym->class == C_GLOBAL || sym->class == C_STATIC || sym->class == C_EXTERN)
    // leaq varname(%rip), %r10
    fprintf(Outfile, "\tleaq\t%s(%%rip), %s\n", sym->name, reglist[r]);
  else
    // leaq -8(%rbp), %r10
    fprintf(Outfile, "\tleaq\t%d(%%rbp), %s\n", sym->st_posn, reglist[r]);

  return r;
}

int cgderef(int r, int type) {
  int newtype = value_at(type);
  int size = cgprimsize(newtype);

  switch (size) {
    case 1:
      // movzbq (%r10), r10
      fprintf(Outfile, "\tmovzbq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    case 4:
      // movslq (%r10), r10
      fprintf(Outfile, "\tmovslq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    case 8:
      // movq (%r10), r10
      fprintf(Outfile, "\tmovq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
  }

  return r;
}

int cgshlconst(int r, int val) {
  // salq $2, %r10
  fprintf(Outfile, "\tsalq\t$%d, %s\n", val, reglist[r]);
  return r;
}

int cgstorderef(int r1, int r2, int type) {
  int size = cgprimsize(type);

  // movq %r8, (%r10)
  switch (size) {
    case 1:
      fprintf(Outfile, "\tmovb\t%s, (%s)\n", breglist[r1], reglist[r2]);
      break;
    case 4:
      fprintf(Outfile, "\tmovl\t%s, (%s)\n", dreglist[r1], reglist[r2]);
      break;
    case 8:
      fprintf(Outfile, "\tmovq\t%s, (%s)\n", reglist[r1], reglist[r2]);
      break;
    default:
      fatald("Can't cgstoderef on type", type);
  }

  return r1;
}

void cgglobstr(int l, char *strvalue, int append) {
  char *cptr;
  if (!append)
    cglabel(l);

  // Loop ends when we hit \0
  for (cptr = strvalue; *cptr; cptr++) {
    fprintf(Outfile, "\t.byte\t%d\n", *cptr);
  }
}

void cgglobstrend(void) {
  fprintf(Outfile, "\t.byte\t0\n");
}

int cgloadglobstr(int label) {
  int r = alloc_register();
  // leaq L2(%%rip), %r10
  fprintf(Outfile, "\tleaq\tL%d(%%rip), %s\n", label, reglist[r]);
  return r;
}

int cgand(int r1, int r2) {
  // andq %r9, %r10
  fprintf(Outfile, "\tandq\t%s, %s\n", reglist[r2], reglist[r1]);
  cgfreereg(r2);
  return r1;
}

int cgor(int r1, int r2) {
  // orq %r9, %r10
  fprintf(Outfile, "\torq\t%s, %s\n", reglist[r2], reglist[r1]);
  cgfreereg(r2);
  return r1;
}

int cgxor(int r1, int r2) {
  // xorq %r9, %r10
  fprintf(Outfile, "\txorq\t%s, %s\n", reglist[r2], reglist[r1]);
  cgfreereg(r2);
  return r1;
}

// Negate a register's value
int cgnegate(int r) {
  // negq %r10
  fprintf(Outfile, "\tnegq\t%s\n", reglist[r]);
  return r;
}

// Invert a register's value
int cginvert(int r) {
  // notq %r10
  fprintf(Outfile, "\tnotq\t%s\n", reglist[r]);
  return r;
}

int cgshl(int r1, int r2) {
  // Amount to shift by has to be loaded in %cl
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshlq\t%%cl, %s\n", reglist[r1]);

  cgfreereg(r2);
  return r1;
}

int cgshr(int r1, int r2) {
  // Amount to shift by has to be loaded in %cl
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshrq\t%%cl, %s\n", reglist[r1]);

  cgfreereg(r2);
  return r1;
}

int cglognot(int r) {
  // AND the register with itself to set the zero
  // flag
  //    test %r9, %r9
  //
  // Set register to 1 if the zero flag is set
  //    sete %r9b
  //
  // Move result to final destination
  //    movzbq %r9b, %r9
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  fprintf(Outfile, "\tsete\t%s\n", breglist[r]);
  fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);

  return r;
}

int cgboolean(int r, int op, int label) {
  fprintf(Outfile, "\ttest\t%s, %s\n", reglist[r], reglist[r]);
  switch (op) {
    case A_IF:
    case A_WHILE:
    case A_LOGAND:
      fprintf(Outfile, "\tje\tL%d\n", label);
      break;
    case A_LOGOR:
      fprintf(Outfile, "\tjne\tL%d\n", label);
      break;
    default:
      // Set if test is not-zero
      // setnz %r9b
      // movzbq %r9b, %r9
      fprintf(Outfile, "\tsetnz\t%s\n", breglist[r]);
      fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);
  }

  return r;
}

int cgstorlocal(int r, struct symtable *sym) {
  switch (cgprimsize(sym->type)) {
    case 1:
      fprintf(Outfile, "\tmovb\t%s, %d(%%rbp)\n", breglist[r],
              sym->st_posn);
      break;
    case 4:
      fprintf(Outfile, "\tmovl\t%s, %d(%%rbp)\n", dreglist[r],
              sym->st_posn);
      break;
    case 8:
      fprintf(Outfile, "\tmovq\t%s, %d(%%rbp)\n", reglist[r],
              sym->st_posn);
      break;
    default:
      fatald("Bad type in cgstorlocal:", sym->type);
  }
  return r;
}

void cgcopyarg(int r, int argposn) {
  // Only 6 arguments will be in registers, the rest
  // should be pushed directly to the stack
  if (argposn > 6) {
    // pushq %r10
    fprintf(Outfile, "\tpushq\t%s\n", reglist[r]);
  } else {
    // +1 because argposn is 1-based
    // movq %r10, %rdi
    fprintf(Outfile, "\tmovq\t%s, %s\n", reglist[r], reglist[FIRSTPARAMREG - argposn + 1]);
  }
  cgfreereg(r);
}

// Given a scalar type, an existing memory offset (which
// has not been allocated to anything yet) and a direction
// (1 is up and -1 is down), calculate and return a suitably
// aligned memory offset for this scalar type.
int cgalign(int type, int offset, int direction) {
  int alignment;

  switch (type) {
    case P_CHAR:
      break;
    default:
      // Align whatever we have now on a 4-byte alignment.
      alignment = 4;
      offset = (offset + direction * (alignment - 1)) & ~(alignment - 1);
  }

  return offset;
}

void cgswitch(int reg, int casecount, int toplabel,
              int *caselabel, int *caseval, int defaultlabel) {
  int i, label;

  label = genlabel();
  cglabel(label);

  // If we have no cases, create a single default case.
  if (casecount == 0) {
    caseval[0] = 0;
    caselabel[0] = defaultlabel;
    casecount = 1;
  }

  // Set up jump table
  // L14:                                  # Switch jump table
  //       .quad   3                       # Three case values
  //       .quad   1, L10                  # case 1: jump to L10
  //       .quad   2, L11                  # case 2: jump to L11
  //       .quad   3, L12                  # case 3: jump to L12
  //       .quad   L13                     # default: jump to L13
  fprintf(Outfile, "\t.quad\t%d\n", casecount);
  for (i = 0; i < casecount; i++)
    fprintf(Outfile, "\t.quad\t%d, L%d\n", caseval[i], caselabel[i]);
  fprintf(Outfile, "\t.quad\tL%d\n", defaultlabel);

  cglabel(toplabel);
  //      movq %r10, %rax           # Load the switch condition in %rax
  //      leaq L14(%rip), %rdx      # Load the base of jump table in rdx
  //      jmp switch
  fprintf(Outfile, "\tmovq\t%s, %%rax\n", reglist[reg]);
  fprintf(Outfile, "\tleaq\tL%d(%%rip), %%rdx\n", label);
  fprintf(Outfile, "\tjmp\t__switch\n");
}

void cgmove(int r1, int r2) {
  // movq %r1, %r2
  fprintf(Outfile, "\tmovq\t%s, %s\n", reglist[r1], reglist[r2]);
}

void cgloadboolean(int r, int val) {
  fprintf(Outfile, "\tmovq\t$%d, %s\n", val, reglist[r]);
}

int cgloadvar(struct symtable *sym, int op) {
  int r, postreg, offset = 1;
  r = alloc_register();

  // If the symbol is a ptr, use the size of the
  // type it points to as any increment or
  // decrement. If not, it's one.
  if (ptrtype(sym->type))
    offset = typesize(value_at(sym->type), sym->ctype);

  // Negate offset for decrements
  if (op == A_PREDEC || op == A_POSTDEC)
    offset = -offset;

  // If we have a pre-op
  if (op == A_PREINC || op == A_PREDEC) {
    // Load the symbol's address
    if (sym->class == C_LOCAL || sym->class == C_PARAM)
      fprintf(Outfile, "\tleaq\t%d(%%rbp), %s\n", sym->st_posn, reglist[r]);
    else
      fprintf(Outfile, "\tleaq\t%s(%%rip), %s\n", sym->name, reglist[r]);

    // Modify the value at the address by that much
    switch (sym->size) {
      case 1:
        fprintf(Outfile, "\taddb\t$%d,(%s)\n", offset, reglist[r]);
        break;
      case 4:
        fprintf(Outfile, "\taddl\t$%d,(%s)\n", offset, reglist[r]);
        break;
      case 8:
        fprintf(Outfile, "\taddq\t$%d,(%s)\n", offset, reglist[r]);
        break;
    }
  }

  if (sym->class == C_LOCAL || sym->class == C_PARAM) {
    switch (sym->size) {
      case 1:
        fprintf(Outfile, "\tmovzbq\t%d(%%rbp), %s\n", sym->st_posn, reglist[r]);
        break;
      case 4:
        fprintf(Outfile, "\tmovslq\t%d(%%rbp), %s\n", sym->st_posn, reglist[r]);
        break;
      case 8:
        fprintf(Outfile, "\tmovq\t%d(%%rbp), %s\n", sym->st_posn, reglist[r]);
    }
  } else {
    switch (sym->size) {
      case 1:
        fprintf(Outfile, "\tmovzbq\t%s(%%rip), %s\n", sym->name, reglist[r]);
        break;
      case 4:
        fprintf(Outfile, "\tmovslq\t%s(%%rip), %s\n", sym->name, reglist[r]);
        break;
      case 8:
        fprintf(Outfile, "\tmovq\t%s(%%rip), %s\n", sym->name, reglist[r]);
    }
  }

  // If we have a post-operation, get a new register
  if (op == A_POSTINC || op == A_POSTDEC) {
    postreg = alloc_register();

    // Load the symbol's address
    if (sym->class == C_LOCAL || sym->class == C_PARAM)
      fprintf(Outfile, "\tleaq\t%d(%%rbp), %s\n", sym->st_posn, reglist[postreg]);
    else
      fprintf(Outfile, "\tleaq\t%s(%%rip), %s\n", sym->name, reglist[postreg]);
    // and change the value at that address

    switch (sym->size) {
      case 1:
        fprintf(Outfile, "\taddb\t$%d,(%s)\n", offset, reglist[postreg]);
        break;
      case 4:
        fprintf(Outfile, "\taddl\t$%d,(%s)\n", offset, reglist[postreg]);
        break;
      case 8:
        fprintf(Outfile, "\taddq\t$%d,(%s)\n", offset, reglist[postreg]);
        break;
    }
    // and free the register
    cgfreereg(postreg);
  }

  // Return the register with the value
  return r;
}
