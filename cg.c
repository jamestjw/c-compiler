#include <stdio.h>
#include <stdlib.h>
#include "data.h"
#include "cg.h"

// List of generic registers that the code will work on
static char *reglist[4] = { "%r8", "%r9", "%r10", "%r11" };
// List of available registers
static int freereg[4];

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

void cgpreamble() {
  freeall_registers();

  fputs(
	  "\t.text\n"
	  ".LC0:\n"
	  "\t.string\t\"%d\\n\"\n"
	  "printint:\n"
	  "\tpushq\t%rbp\n"
	  "\tmovq\t%rsp, %rbp\n"
	  "\tsubq\t$16, %rsp\n"
	  "\tmovl\t%edi, -4(%rbp)\n"
	  "\tmovl\t-4(%rbp), %eax\n"
	  "\tmovl\t%eax, %esi\n"
	  "\tleaq	.LC0(%rip), %rdi\n"
	  "\tmovl	$0, %eax\n"
	  "\tcall	printf@PLT\n"
	  "\tnop\n"
	  "\tleave\n"
	  "\tret\n"
	  "\n"
	  "\t.globl\tmain\n"
	  "\t.type\tmain, @function\n"
	  "main:\n"
	  "\tpushq\t%rbp\n"
	  "\tmovq	%rsp, %rbp\n",
    Outfile);
}

void cgpostamble() {
  fputs(
      "\tmovl $0, %eax\n"
      "\tpopq %rbp\n"
      "\tret\n",
      Outfile
      );
}

// Allocates a register and loads a literal into it.
int cgload(int value) {
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

