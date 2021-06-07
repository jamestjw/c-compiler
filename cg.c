#include <stdio.h>
#include <stdlib.h>
#include "data.h"
#include "cg.h"
#include "misc.h"
#include "types.h"

// Flag to say which section were are outputting in to
enum { no_seg, text_seg, data_seg } currSeg = no_seg;

#define NUMFREEREGS 4
#define FIRSTPARAMREG 9   // Position of first parameter register

// List of generic registers that the code will work on
static char *reglist[] =
  { "%r10", "%r11", "%r12", "%r13", "%r9", "%r8", "%rcx", "%rdx", "%rsi",
"%rdi" };
// Lower 8 bits of registers in reglist
static char *breglist[] =
  { "%r10b", "%r11b", "%r12b", "%r13b", "%r9b", "%r8b", "%cl", "%dl", "%sil",
"%dil" };
// Lower 4 bytes of registers in reglist
static char *dreglist[] =
  { "%r10d", "%r11d", "%r12d", "%r13d", "%r9d", "%r8d", "%ecx", "%edx",
"%esi", "%edi" };

// List of available registers
static int freereg[NUMFREEREGS];

// List of comparison instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] = { "sete", "setne", "setl", "setg", "setle", "setge" };

// List of inverted jump instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = { "jne", "je", "jge", "jle", "jg", "jl" };

// Position of next local variable relative to stack base pointer
static int localOffset;
static int stackOffset;

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
void freeall_registers(void) {
  freereg[0] = freereg[1] = freereg[2] = freereg[3] = 1;
}

// Allocate a free register and returns the corresponding
// identifier. Program exits if no available registers remain.
static int alloc_register(void) {
  for (int i = 0; i < 4; ++i) {
    if (freereg[i]) {
      freereg[i] = 0;
      return i;
    }
  }

  fprintf(stderr, "Out of free registers!\n");
  exit(1);
}

// Frees up a previously allocated register.
static void free_register(int reg) {
  if (freereg[reg] != 0) {
    fprintf(stderr, "Error trying to free register %d\n", reg);
    exit(1);
  }
  freereg[reg] = 1;
}

static int newlocaloffset(int type) {
  // Decrement by a minimum of 4 bytes
  localOffset += (cgprimsize(type) > 4) ? cgprimsize(type) : 4;
  return -localOffset;
}

void cgpreamble() {
  freeall_registers();
}

void cgfuncpreamble(int id) {
  char *name = Symtable[id].name;
  int i;
  int paramOffset = 16;           // Offset of first param (relative to %rbp)
  int paramReg = FIRSTPARAMREG;   // Index to first param register

  // Generate output in text segment
  cgtextseg();
  // Reset offset
  localOffset = 0;

  // Output the function start, save the %rsp and %rsp
  fprintf(Outfile,
          "\t.globl\t%s\n"
          "\t.type\t%s, @function\n"
          "%s:\n" "\tpushq\t%%rbp\n"
          "\tmovq\t%%rsp, %%rbp\n", name, name, name);

  // Copy in-register parameters to the stack
  for (i = NSYMBOLS - 1; i > Locls; i--) {
    // Stop if we encounter a local var
    if (Symtable[i].class != C_PARAM)
      break;
    // Only 6 params are passed in registers
    if (i < NSYMBOLS - 6)
      break;
    Symtable[i].posn = newlocaloffset(Symtable[i].type);
    cgstorlocal(paramReg--, i);
  }

  // For the remainder, if they are a parameter than they are
  // already on the stack above %rbp, hence we just take note
  // of their position. For locals, we shall create a new 
  // stack position.
  for (; i > Locls; i--)  {
    if (Symtable[i].class == C_PARAM) {
      Symtable[i].posn = paramOffset;
      paramOffset += 8;
    } else {
      Symtable[i].posn = newlocaloffset(Symtable[i].type);
    }
  }

  // Align stack pointer to be a multiple of 16
  stackOffset = (localOffset + 15) & ~15;
  // Decrement stack pointer based on how many 
  // variables we loaded onto the stack
  fprintf(Outfile, "\taddq\t$%d, %%rsp\n", -stackOffset);
}

void cgfuncpostamble(int id) {
  cglabel(Symtable[id].endlabel);
  // Restore stack pointer
  fprintf(Outfile, "\taddq\t$%d, %%rsp\n", stackOffset);
  fputs(
      "\tpopq %rbp\n"
      "\tret\n",
      Outfile
      );
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
  fprintf(Outfile, "\taddq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);

  return r2;
}

// Multiplies two registers and saves the result in one of them
// while freeing the other.
int cgmul(int r1, int r2) {
  // e.g. imulq %r8, %r9
  fprintf(Outfile, "\timulq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);

  return r2;
}

// Subtracts the second register from the first and returns
// the register with the result while freeing the other.
int cgsub(int r1, int r2) {
  // e.g. subq %r2, %r1
  fprintf(Outfile, "\tsubq\t%s, %s\n", reglist[r2], reglist[r1]);
  free_register(r2);

  return r1;
}

// Divide first register by the second and returns the
// register containing the result while freeing the other.
int cgdiv(int r1, int r2) {
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

  // Move result from %rax to %r1
  // e.g. movq %rax, %r1
  fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[r1]);
  free_register(r2);

  return r1;
}

// Calls the printint function in the preamble to
// print an integer
void cgprintint(int r) {
  // Linux x86-64 expects the first argument to be in %rdi
  fprintf(Outfile, "\tmovq\t%s, %%rdi\n", reglist[r]);
  fprintf(Outfile, "\tcall\tprintint\n");
  free_register(r);
}

int cgloadglob(int id, int op) {
  char *name = Symtable[id].name;
  int r = alloc_register();

  switch (cgprimsize(Symtable[id].type)) {
    case 1:
      if (op == A_PREINC)
        // incb identifier(%rip)
        fprintf(Outfile, "\tincb\t%s(\%%rip)\n", name);
      if (op == A_PREDEC)
        // decb identifier(%rip)
        fprintf(Outfile, "\tdecb\t%s(\%%rip)\n", name);
      // e.g. movzbq identifier(%rip), %r8

      fprintf(Outfile, "\tmovzbq\t%s(\%%rip), %s\n", name, reglist[r]);

      if (op == A_POSTINC)
        // incb identifier(%rip)
        fprintf(Outfile, "\tincb\t%s(\%%rip)\n", name);
      if (op == A_POSTDEC)
        // decb identifier(%rip)
        fprintf(Outfile, "\tdecb\t%s(\%%rip)\n", name);
      break;
    case 4:
      if (op == A_PREINC)
        // incl identifier(%rip)
        fprintf(Outfile, "\tincl\t%s(\%%rip)\n", name);
      if (op == A_PREDEC)
        // decl identifier(%rip)
        fprintf(Outfile, "\tdecl\t%s(\%%rip)\n", name);
      // e.g. movzbl identifier(%rip), %r8

      fprintf(Outfile, "\tmovslq\t%s(\%%rip), %s\n", name, reglist[r]);

      if (op == A_POSTINC)
        // incl identifier(%rip)
        fprintf(Outfile, "\tincl\t%s(\%%rip)\n", name);
      if (op == A_POSTDEC)
        // decl identifier(%rip)
        fprintf(Outfile, "\tdecl\t%s(\%%rip)\n", name);
      break;
    case 8:
      if (op == A_PREINC)
        // incq identifier(%rip)
        fprintf(Outfile, "\tincq\t%s(\%%rip)\n", name);
      if (op == A_PREDEC)
        // decq identifier(%rip)
        fprintf(Outfile, "\tdecq\t%s(\%%rip)\n", name);
      // e.g. movq identifier(%rip), %r8

      fprintf(Outfile, "\tmovq\t%s(\%%rip), %s\n", name, reglist[r]);

      if (op == A_POSTINC)
        // incq identifier(%rip)
        fprintf(Outfile, "\tincq\t%s(\%%rip)\n", name);
      if (op == A_POSTDEC)
        // decq identifier(%rip)
        fprintf(Outfile, "\tdecq\t%s(\%%rip)\n", name);
      break;
    default:
      fatald("Bad type in cgloadglob", Symtable[id].type);
  }

  return r;
}

int cgstorglob(int r, int id) {
  switch (cgprimsize(Symtable[id].type)) {
    case 1:
      // Only move a single byte for chars
      // e.g. movb %r10b, identifier(%rip)
      fprintf(Outfile, "\tmovb\t%s, %s(\%%rip)\n", breglist[r], Symtable[id].name);
      break;
    case 4:
      // e.g. movl %r10d, identifier(%rip)
      fprintf(Outfile, "\tmovl\t%s, %s(\%%rip)\n", dreglist[r], Symtable[id].name);
      break;
    case 8:
      // e.g. movq %r10, identifier(%rip)
      fprintf(Outfile, "\tmovq\t%s, %s(\%%rip)\n", reglist[r], Symtable[id].name);
      break;
    default:
      fatald("Bad type in cgloadglob", Symtable[id].type);
  }

  return r;
}

void cgglobsym(int id) {
  int typesize;

  if (Symtable[id].stype == S_FUNCTION)
    return;

  typesize = cgprimsize(Symtable[id].type);
   
  // .data
  // .globl varname
  // varname:   .long   0
  //            .long   0
  //            .long   0
  cgdataseg();
  fprintf(Outfile, "\t.globl\t%s\n", Symtable[id].name);
  fprintf(Outfile, "%s:", Symtable[id].name);

  for (int i = 0; i < Symtable[id].size; i++) {
     switch (typesize) {
       case 1: fprintf(Outfile, "\t.byte\t0\n"); break; 
       case 4: fprintf(Outfile, "\t.long\t0\n"); break; 
       case 8: fprintf(Outfile, "\t.quad\t0\n"); break; 
       default: fatald("Unknown typesize in cgglobsym", typesize);       
     } 
 }                                                                    
}

int cgcompare_and_set(int ASTop, int r1, int r2) {
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  // cmpq %r2, %r1
  // This calculates %r1 - %r2
  fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
  // setge %r10b
  // This only sets the lowest byte of the register
  // Note: These instructions only works on 8-bit registers
  fprintf(Outfile, "\t%s\t%s\n", cmplist[ASTop - A_EQ], breglist[r2]);
  // movzbq %r10b, %r10
  // Moves the lowest bytes from one register and extends it to fill
  // a 64-bit register
  fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r2], reglist[r2]);

  free_register(r1);
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

int cgcompare_and_jump(int ASTop, int r1, int r2, int label) {
   if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_jump()");
  
   // cmpq %r2, %r1
   fprintf(Outfile, "\tcmpq\t%s, %s\n", reglist[r2], reglist[r1]);
   // jne L1
   fprintf(Outfile, "\t%s\tL%d\n", invcmplist[ASTop - A_EQ], label);
   freeall_registers();
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
    case P_CHAR: return 1;
    case P_INT: return 4;
    case P_LONG: return 8;
    default:
      fatald("Bad type in cgprimsize", type);
  }

  // Handle -Wall violation
  return 0;
}

int cgcall(int id, int numargs) {
  int outr = alloc_register();

  // call funcname
  fprintf(Outfile, "\tcall\t%s\n", Symtable[id].name);

  // Remove arguments pushed to the stack
  if (numargs > 6)
    // addq $16, %rsp
    fprintf(Outfile, "\taddq\t$%d, %%rsp\n", 8 * (numargs - 6));

  // Move return code from %rax
  // movq %rax, %r9
  fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[outr]);

  return outr;
}

void cgreturn(int reg, int id) {
  // Move return value to %rax
  switch (Symtable[id].type) {
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
      fatald("Bad function type in cgreturn", Symtable[id].type);
  }

  cgjump(Symtable[id].endlabel);
}

int cgaddress(int id) {
  int r = alloc_register();

  if (Symtable[id].class == C_LOCAL)
    // leaq -8(%rbp), %r10
    fprintf(Outfile, "\tleaq\t%d(%%rbp), %s\n", Symtable[id].posn, reglist[r]); 
  else
    // leaq varname(%rip), %r10
    fprintf(Outfile, "\tleaq\t%s(%%rip), %s\n", Symtable[id].name, reglist[r]);
  
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
    case 2:
      // movzwq (%r10), r10
      fprintf(Outfile, "\tmovzwq\t(%s), %s\n", reglist[r], reglist[r]);
      break;
    case 4:
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
    case 2:
    case 4:
    case 8:
      fprintf(Outfile, "\tmovq\t%s, (%s)\n", reglist[r1], reglist[r2]);
      break;
    default:
      fatald("Can't cgstoderef on type", type);
  }

  return r1;
}

void cgglobstr(int l, char *strvalue) {
  char *cptr;
  cglabel(l);
  
  // Loop ends when we hit \0
  for (cptr = strvalue; *cptr; cptr++) {
    fprintf(Outfile, "\t.byte\t%d\n", *cptr);
  }
  fprintf(Outfile, "\t.byte\t0\n");
}

int cgloadglobstr(int id) {
  int r = alloc_register();
  // leaq L2(%%rip), %r10
  fprintf(Outfile, "\tleaq\tL%d(\%%rip), %s\n", id, reglist[r]);
  return r;
}

int cgand(int r1, int r2) {
  // andq %r9, %r10
  fprintf(Outfile, "\tandq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return r2;
}

int cgor(int r1, int r2) {
  // orq %r9, %r10
  fprintf(Outfile, "\torq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1);
  return r2;
}

int cgxor(int r1, int r2) {
  // xorq %r9, %r10
  fprintf(Outfile, "\txorq\t%s, %s\n", reglist[r1], reglist[r2]);
  free_register(r1); 
  return r2;
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

  free_register(r2);
  return r1;
}

int cgshr(int r1, int r2) {
  // Amount to shift by has to be loaded in %cl
  fprintf(Outfile, "\tmovb\t%s, %%cl\n", breglist[r2]);
  fprintf(Outfile, "\tshrq\t%%cl, %s\n", reglist[r1]);

  free_register(r2);
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
  if (op == A_IF || op == A_WHILE)
    // je L2
    fprintf(Outfile, "\tje\tL%d\n", label);
  else {
    // Set if test is not-zero
    // setnz %r9b
    // movzbq %r9b, %r9
    fprintf(Outfile, "\tsetnz\t%s\n", breglist[r]);
    fprintf(Outfile, "\tmovzbq\t%s, %s\n", breglist[r], reglist[r]);
  }

  return r;
}

int cgloadlocal(int id, int op) {
  // Get a new register
  int r = alloc_register();

  // Print out the code to initialise it
  switch (cgprimsize(Symtable[id].type)) {
    case 1:
      if (op == A_PREINC)
	      fprintf(Outfile, "\tincb\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_PREDEC)
	      fprintf(Outfile, "\tdecb\t%d(%%rbp)\n", Symtable[id].posn);

      fprintf(Outfile, "\tmovzbq\t%d(%%rbp), %s\n", Symtable[id].posn, reglist[r]);

      if (op == A_POSTINC)
	      fprintf(Outfile, "\tincb\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_POSTDEC)
	      fprintf(Outfile, "\tdecb\t%d(%%rbp)\n", Symtable[id].posn);
      break;
    case 4:
      if (op == A_PREINC)
	      fprintf(Outfile, "\tincl\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_PREDEC)
	      fprintf(Outfile, "\tdecl\t%d(%%rbp)\n", Symtable[id].posn);
     
      fprintf(Outfile, "\tmovslq\t%d(%%rbp), %s\n", Symtable[id].posn, reglist[r]);

      if (op == A_POSTINC)
	      fprintf(Outfile, "\tincl\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_POSTDEC)
	      fprintf(Outfile, "\tdecl\t%d(%%rbp)\n", Symtable[id].posn);
      break;
    case 8:
      if (op == A_PREINC)
	      fprintf(Outfile, "\tincq\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_PREDEC)
	      fprintf(Outfile, "\tdecq\t%d(%%rbp)\n", Symtable[id].posn);

      fprintf(Outfile, "\tmovq\t%d(%%rbp), %s\n", Symtable[id].posn, reglist[r]);

      if (op == A_POSTINC)
	      fprintf(Outfile, "\tincq\t%d(%%rbp)\n", Symtable[id].posn);
      if (op == A_POSTDEC)
	      fprintf(Outfile, "\tdecq\t%d(%%rbp)\n", Symtable[id].posn);
      break;
    default:
      fatald("Bad type in cgloadlocal:", Symtable[id].type);
  }
  return r;  
}

int cgstorlocal(int r, int id) {
  switch (cgprimsize(Symtable[id].type)) {
    case 1:
      fprintf(Outfile, "\tmovb\t%s, %d(%%rbp)\n", breglist[r],
	      Symtable[id].posn);
      break;
    case 4:
      fprintf(Outfile, "\tmovl\t%s, %d(%%rbp)\n", dreglist[r],
	      Symtable[id].posn);
      break;
    case 8:
      fprintf(Outfile, "\tmovq\t%s, %d(%%rbp)\n", reglist[r],
	      Symtable[id].posn);
      break;
    default:
      fatald("Bad type in cgstorlocal:", Symtable[id].type);
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
}
