void freeall_registers(int keepreg);
void cgpreamble();
void cgpostamble();
void cgfuncpreamble(struct symtable *sym);
void cgfuncpostamble(struct symtable *sym);

int cgloadint(int value, int type);
int cgadd(int r1, int r2);
int cgsub(int r1, int r2);
int cgmul(int r1, int r2);
int cgdivmod(int r1, int r2, int op);
// Load a value to a global symbol
int cgstorglob(int r, struct symtable *sym);

// Define a global symbol
void cgglobsym(struct symtable *sym);
void cgprintint(int r);

// Compares values in two registers based on the
// given operator and returns a register containing
// either 0 or 1 depending on the result of the comparison
int cgcompare_and_set(int ASTop, int r1, int r2, int type);
// Compares values in two registers and jumps to the label
// if the comparison is false
int cgcompare_and_jump(int ASTop, int r1, int r2, int label, int type);

// Generates a jump to a given label
void cgjump(int l);
// Generates a label, e.g. L1
void cglabel(int l);

// Widen the value in the register from the old
// to the new type, and return a register with the
// new value
int cgwiden(int r, int oldtype, int newtype);

// Return the size of a primitive type
int cgprimsize(int type);
// Call a function with one argument from the
// given register and return a register with the
// result of the function call
int cgcall(struct symtable *sym, int numargs);
// Return from a function call while returning a
// value in the passed register
void cgreturn(int reg, struct symtable *sym);
// Generate code to load address of a global identifier
// into a variable and returning the register that contains
// it
int cgaddress(struct symtable *sym);
// Dereference a pointer and loading the value into the
// same register
int cgderef(int r, int type);
// Shift bits to the left by a constant and return the
// register containing the result
int cgshlconst(int r, int val);
// Store a value through a dereferenced pointer
int cgstorderef(int r1, int r2, int type);
// Generate code for a string literal to a given label
void cgglobstr(int l, char *strvalue, int append);
// Mark the end of string, e.g. null terminate it
void cgglobstrend(void);
// Return register containing pointer to a string
// literal of this ID
int cgloadglobstr(int label);
int cgand(int r1, int r2);
int cgor(int r1, int r2);
int cgxor(int r1, int r2);
int cgnegate(int r);
int cginvert(int r);
int cgshl(int r1, int r2);
int cgshr(int r1, int r2);
int cglognot(int r);
int cgboolean(int r, int op, int label);
// Reset offset of local variables when parsing a new function
void cgresetlocals(void);
int cgloadvar(struct symtable *sym, int op);
// Store a register's value into a local variable
int cgstorlocal(int r, struct symtable *sym);
void cgcopyarg(int r, int argposn);
int cgalign(int type, int offset, int direction);
void cgswitch(int reg, int casecount, int toplabel,
              int *caselabel, int *caseval, int defaultlabel);
int alloc_register(void);
void cgmove(int r1, int r2);
// Logically OR two registers and return a
// register with the result, 1 or 0
int cglogor(int r1, int r2);
// Logically AND two registers and return a register with
// the result, 1 or 0
int cglogand(int r1, int r2);
void cgloadboolean(int r, int val);
void spill_all_regs(void);
void cgfreereg(int reg);