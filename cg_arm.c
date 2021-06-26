#include <stdio.h>
#include <stdlib.h>
#include "data.h"
#include "cg.h"
#include "misc.h"

// We have to store large integer literals in memory
// Keep a list of them to be output in the postamble
#define MAXINTS 1024
int Intlist[MAXINTS];
static int Intslot = 0;

// List of generic registers that the code will work on
static char *reglist[4] = { "r4", "r5", "r6", "r7" };
// List of available registers
static int freereg[4];

// List of comparison instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *cmplist[] = { "moveq", "movne", "movlt", "movgt", "movle", "movge" };

// List of inverted comparison instructions corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *invcmplist[] = { "movne", "moveq", "movge", "movle", "movgt", "movlt" };

// List of branching operators corresponding to
// A_EQ, A_NE, A_LT, A_GT, A_LE, A_GE
static char *brlist[] = { "bne", "beq", "bge", "ble", "bgt", "blt" };

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
  4, // P_LONG
  4, // P_CHARPTR
  4, // P_INTPTR
  4, // P_LONGPTR
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

static void set_var_offset(int id) {
  int offset = 0;

  for (int i = 0; i < id; i++) {
    if (n->sym->stype == S_VARIABLE)
      offset += 4;
  }

  // ldr r3, .L2+4
  fprintf(Outfile, "\tldr\tr3, .L2+%d\n", offset);
}

// Load addr of integer into r3 and also set up the integer
// to be defined separately in the code
static void set_int_offset(int val) {
  int offset = -1;

  // Check if the int is in the list
  for (int i = 0; i < Intslot; i++) {
    if (Intlist[i] == val) {
      offset = 4 * i;
      break;
    }
  }

  // If not in the list, add it
  if (offset == -1) {
    offset = 4 * Intslot;
    if (Intslot == MAXINTS)
      fatal("Out of int slots in set_int_offset()");
    Intlist[Intslot++] = val;
  }

  // Load to r3 with this offset
  // ldr r3, .L3+4
  fprintf(Outfile, "\tldr\tr3, .L3+%d\n", offset);
}

void cgpreamble() {
  freeall_registers(NOREG);

  fputs("\t.text\n", Outfile);
}

void cgfuncpreamble(char *name) {
    //   .text
    //   .globl        main
    //   .type         main, %function
    //   main:         push  {fp, lr}          # Save the frame pointer of calling function and return address
    //                 add   fp, sp, #4        # Set up frame pointer for this function
    //                 sub   sp, sp, #8        # Lower the stack pointer by 8
    //                 str   r0, [fp, #-8]     # Copy argument to stack

    fprintf(Outfile,
          "\t.text\n"
          "\t.globl\t%s\n"
          "\t.type\t%s, %%function\n"
          "%s:\n"
          "\tpush\t{fp, lr}\n"
          "\tadd\tfp, sp, #4\n"
          "\tsub\tsp, sp, #8\n"
          "\tstr\tr0, [fp, #-8]\n", name, name, name);
}

void cgfuncpostamble(int id) {
  cglabel(n->sym->st_endlabel);
  // sub sp, fp, #4
  // pop {fp, pc}
  // .align 2
  fputs(
      "\tsub\tsp, fp, #4\n"
      "\tpop\t{fp, pc}\n"
      "\t.align\t2\n", Outfile);
}

void cgpostamble() {
  // Handle global variables
  //
  // .L2:
  //    .word var1
  //    .word var2
  fprintf(Outfile, ".L2:\n");
  for (int i = 0; i < Globs; i++) {
    if (n->sym->stype == S_VARIABLE)
      fprintf(Outfile, "\t.word %s\n", n->sym->name);
  }

  // Handle integer literals
  //
  // .L3:
  //    .word 5
  //    .word 17
  fprintf(Outfile, ".L3:\n");
  for (int i = 0; i < Intslot; i++) {
    fprintf(Outfile, "\t.word %d\n", Intlist[i]);
  }
}

// Allocates a register and loads an integer literal into it.
int cgloadint(int value, int type) {
  int r = alloc_register();

  // For small values, do it with one instruction
  if (value <= 1000)
    // mov r4, #10
    fprintf(Outfile, "\tmov\t%s, #%d\n", reglist[r], value);
  else {
    set_int_offset(value);
    // Load integer from r3
    // ldr r4, [r3]
    fprintf(Outfile, "\tldr\t%s, [r3]\n", reglist[r]);
  }

  return r;
}

// Adds two registers and saves the result in one of them
// while freeing the other.
int cgadd(int r1, int r2) {
  // e.g. add r2, r1, r2
  fprintf(Outfile, "\tadd\t%s, %s, %s\n", reglist[r2], reglist[r1], reglist[r2]);
  free_register(r1);

  return r2;
}

// Multiplies two registers and saves the result in one of them
// while freeing the other.
int cgmul(int r1, int r2) {
  // e.g. mul r2, r1, r2
  fprintf(Outfile, "\tmul\t%s, %s, %s\n", reglist[r2], reglist[r1], reglist[r2]);
  free_register(r1);

  return r2;
}

// Subtracts the second register from the first and returns
// the register with the result while freeing the other.
int cgsub(int r1, int r2) {
  // e.g. sub r2, r1, r2
  fprintf(Outfile, "\tsub\t%s, %s, %s\n", reglist[r2], reglist[r1], reglist[r2]);
  free_register(r2);

  return r1;
}

// Divide first register by the second and returns the
// register containing the result while freeing the other.
int cgdivmod(int r1, int r2) {
  // Place dividend in r0, and divisor in r1

  // mov r0, r4
  // mov r1, r5
  fprintf(Outfile, "\tmov\tr0, %s\n", reglist[r1]);
  fprintf(Outfile, "\tmov\tr1, %s\n", reglist[r2]);

  // Quotient will be in r0
  // bl __aeabi_idiv
  // mov r4, r0
  fprintf(Outfile, "\tbl\t__aeabi_idiv\n");
  fprintf(Outfile, "\tmov\t%s, r0\n", reglist[r1]);

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

  // Load variable addr into r3
  set_var_offset(id);

  // Use addr in r3 to load value to a register
  // ldr r4, [r3]
  switch (n->sym->type) {
    case P_CHAR:
      fprintf(Outfile, "\tldrb\t%s, [r3]\n", reglist[r]);
      break;
    case P_INT:
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      fprintf(Outfile, "\tldr\t%s, [r3]\n", reglist[r]);
      break;
    default:
      fatald("Bad type in cgloadglob:", n->sym->type);
  }

  return r;
}

int cgstorglob(int r, int id) {
  // Get address to the variable
  set_var_offset(id);

  // Store value to address in r3
  switch (n->sym->type) {
    case P_CHAR:
      // e.g. strb r4, [r3]
      fprintf(Outfile, "\tstrb\t%s, [r3]\n", reglist[r]);
      break;
    case P_INT:
    case P_LONG:
    case P_CHARPTR:
    case P_INTPTR:
    case P_LONGPTR:
      // e.g. str r4, [r3]
      fprintf(Outfile, "\tstr\t%s, [r3]\n", reglist[r]);
      break;
    default:
      fatald("Bad type in cgloadglob", n->sym->type);
  }

  return r;
}

void cgglobsym(int id) {
  int typesize;

  typesize = cgprimsize(n->sym->type);

  // .data
  // .globl varname
  // varname: .long 0
  fprintf(Outfile, "\t.data\n" "\t.globl\t%s\n", n->sym->name);
  switch(typesize) {
    case 1: fprintf(Outfile, "%s:\t.byte\t0\n", n->sym->name); break;
    case 4: fprintf(Outfile, "%s:\t.long\t0\n", n->sym->name); break;
    default: fatald("Unknown typesize in cgglobsym", typesize);
  }
}

int cgcompare_and_set(int ASTop, int r1, int r2) {
  if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_set()");

  // cmp r1, r2
  fprintf(Outfile, "\tcmp\t%s, %s\n", reglist[r1], reglist[r2]);
  // Set lower byte to 1 if condition fulfilled
  // moveq r2, #1
  fprintf(Outfile, "\t%s\t%s, #1\n", cmplist[ASTop - A_EQ], reglist[r2]);
  // Set lower byte to 0 if opposite condition fulfilled
  // movne r2, #0
  fprintf(Outfile, "\t%s\t%s, #0\n", invcmplist[ASTop - A_EQ], reglist[r2]);
  // Sign extension
  // uxtb r2, r2
  fprintf(Outfile, "\tuxtb\t%s, %s\n", reglist[r2], reglist[r2]);

  free_register(r1);
  return r2;
}

void cglabel(int l) {
  // L1:
  fprintf(Outfile, "L%d:\n", l);
}

void cgjump(int l) {
  // b L1
  fprintf(Outfile, "\tb\tL%d\n", l);
}

int cgcompare_and_jump(int ASTop, int r1, int r2, int label) {
   if (ASTop < A_EQ || ASTop > A_GE)
    fatal("Bad ASTop in cgcompare_and_jump()");

   // cmpq r1, r2
   fprintf(Outfile, "\tcmp\t%s, %s\n", reglist[r1], reglist[r2]);
   // bne L1
   fprintf(Outfile, "\t%s\tL%d\n", brlist[ASTop - A_EQ], label);
   freeall_registers(NOREG);
   return NOREG;
}

int cgwiden(int r, int oldtype, int newtype) {
  // cgloadglob has already taken care of the widening
  // of P_CHAR variables, hence we have nothing to do
  // here.
  return r;
}

int cgprimsize(int type) {
  if (type < P_NONE || type > P_LONGPTR)
    fatal("Bad type in cgprimsize()");

  return psize[type];
}

int cgcall(int r, int id) {
  // Move argument to r0
  // mov r0, r4
  fprintf(Outfile, "\tmov\tr0, %s\n", reglist[r]);
  // bl funcname
  fprintf(Outfile, "\tbl\t%s\n", n->sym->name);
  // Move return value from r0
  // mov r4, r0
  fprintf(Outfile, "\tmov\t%s, r0\n", reglist[r]);

  return r;
}

void cgreturn(int reg, int id) {
  // Move return value to r0
  // mov r0, r4
  fprintf(Outfile, "\tmov\tr0, %s\n", reglist[reg]);
  cgjump(n->sym->st_endlabel);
}

int cgaddress(int id) {
  int r = alloc_register();
  set_var_offset(id);
  // mov r4, r4
  fprintf(Outfile, "\tmov\t%s, r3\n", reglist[r]);
  return r;
}

int cgderef(int r, int type) {
  switch (type) {
    case P_CHARPTR:
      // ldrb r5, [r4]
      fprintf(Outfile, "\tldrb\t%s, [%s]\n", reglist[r], reglist[r]);
      break;
    case P_INTPTR:
      // ldr r5, [r4]
      fprintf(Outfile, "\tldr\t%s, [%s]\n", reglist[r], reglist[r]);
      break;
    case P_LONGPTR:
      // ldr r5, [r4]
      fprintf(Outfile, "\tldr\t%s, [%s]\n", reglist[r], reglist[r]);
      break;
  }

  return r;
}

int cgshlconst(int r, int val) {
  // lsl r5, r5, #2
  fprintf(Outfile, "\tlsl\t%s, %s, #%d\n", reglist[r], reglist[r], val);
  return r;
}

int cgstorderef(int r1, int r2, int type) {
  // str r4, [r5]
  switch (type) {
    case P_CHAR:
      fprintf(Outfile, "\tstrb\t%s, [%s]\n", reglist[r1], reglist[r2]);
      break;
    case P_INT:
    case P_LONG:
      fprintf(Outfile, "\tstr\t%s, [%s]\n", reglist[r1], reglist[r2]);
      break;
    default:
      fatald("Can't cgstorderef on type", type);
  }

  return r1;
}

