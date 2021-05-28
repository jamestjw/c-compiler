#include <stdio.h>
#include <stdlib.h>
#include "data.h"
#include "cg.h"
#include "misc.h"

// List of generic registers that the code will work on
static char *reglist[4] = { "%r8", "%r9", "%r10", "%r11" };
// Lower 8 bits of registers in reglist
static char *breglist[4] = { "%r8b", "%r9b", "%r10b", "%r11b" }; 
// Lower 4 bytes of registers in reglist
static char *dreglist[4] = { "%r8d", "%r9d", "%r10d", "%r11d" }; 
// List of available registers
static int freereg[4];

// List of comparison instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] = { "sete", "setne", "setl", "setg", "setle", "setge" };

// List of inverted jump instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = { "jne", "je", "jge", "jle", "jg", "jl" };

// Mark all registers as available
void freeall_registers(void) {
  freereg[0] = freereg[1] = freereg[2] = freereg[3] = 1;
}

// Array of sizes of primitives
static int psize[] = {
  0, // P_NONE 
  0, // P_VOID
  1, // P_CHAR
  4, // P_INT
  8, // P_LONG
};

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

void cgpreamble() {
  freeall_registers();

  fputs("\t.text\n", Outfile);
}

void cgfuncpreamble(char *name) {
    fprintf(Outfile,
          "\t.text\n"
          "\t.globl\t%s\n"
          "\t.type\t%s, @function\n"
          "%s:\n" "\tpushq\t%%rbp\n"
          "\tmovq\t%%rsp, %%rbp\n", name, name, name);
}

void cgfuncpostamble(int id) {
  cglabel(Gsym[id].endlabel);
  fputs(
      "\tpopq %rbp\n"
      "\tret\n",
      Outfile
      );
}

void cgpostamble() {
  fputs(
      "\tmovl $0, %eax\n"
      "\tpopq %rbp\n"
      "\tret\n",
      Outfile
      );
}

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

int cgloadglob(int id) {
  int r = alloc_register();

  switch (Gsym[id].type) {
    case P_CHAR:
      // e.g. movzbq identifier(%rip), %r8
      fprintf(Outfile, "\tmovzbq\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
      break;
    case P_INT:
      // e.g. movzbl identifier(%rip), %r8
      fprintf(Outfile, "\tmovzbl\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
      break;
    case P_LONG:
      // e.g. movq identifier(%rip), %r8
      fprintf(Outfile, "\tmovq\t%s(\%%rip), %s\n", Gsym[id].name, reglist[r]);
      break;
    default:
      fatald("Bad type in cgloadglob", Gsym[id].type);
  }

  return r;
}

int cgstorglob(int r, int id) {
  switch (Gsym[id].type) {
    case P_CHAR:
      // Only move a single byte for chars
      // e.g. movb %r10b, identifier(%rip)
      fprintf(Outfile, "\tmovb\t%s, %s(\%%rip)\n", breglist[r], Gsym[id].name);
      break;
    case P_INT:
      // e.g. movl %r10d, identifier(%rip)
      fprintf(Outfile, "\tmovl\t%s, %s(\%%rip)\n", dreglist[r], Gsym[id].name);
      break;
    case P_LONG:
      // e.g. movq %r10, identifier(%rip)
      fprintf(Outfile, "\tmovq\t%s, %s(\%%rip)\n", reglist[r], Gsym[id].name);
      break;
    default:
      fatald("Bad type in cgloadglob", Gsym[id].type);
  }

  return r;
}

void cgglobsym(int id) {
  int typesize;

  typesize = cgprimsize(Gsym[id].type);

  // e.g. .comm var_name,8,8
  fprintf(Outfile, "\t.comm\t%s,%d,%d\n", Gsym[id].name, typesize, typesize);
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
  if (type < P_NONE || type > P_LONG)
    fatal("Bad type in cgprimsize()");

  return psize[type];
}

int cgcall(int r, int id) {
  int outr = alloc_register();

  // Move argument to %rdi
  // movq %r8, %rdi
  fprintf(Outfile, "\tmovq\t%s, %%rdi\n", reglist[r]);
  // call funcname
  fprintf(Outfile, "\tcall\t%s\n", Gsym[id].name);
  // Move return code from %rax
  // movq %rax, %r9
  fprintf(Outfile, "\tmovq\t%%rax, %s\n", reglist[outr]);

  free_register(r);

  return outr;
}

void cgreturn(int reg, int id) {
  // Move return value to %rax
  switch (Gsym[id].type) {
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
      fatald("Bad function type in cgreturn", Gsym[id].type);
  }

  cgjump(Gsym[id].endlabel);
}
