void freeall_registers(void);
void cgpreamble();
void cgpostamble();
void cgfuncpreamble(char *name);
void cgfuncpostamble();

int cgloadint(int value);
int cgadd(int r1, int r2);
int cgsub(int r1, int r2);
int cgmul(int r1, int r2);
int cgdiv(int r1, int r2);
// Load a value from a global symbol
int cgloadglob(char *identifier);
// Load a value to a global symbol
int cgstorglob(int r, char *identifier);

// Define a global symbol
void cgglobsym(char *sym);
void cgprintint(int r);

// Compares values in two registers based on the
// given operator and returns a register containing
// either 0 or 1 depending on the result of the comparison
int cgcompare_and_set(int ASTop, int r1, int r2);
// Compares values in two registers and jumps to the label
// if the comparison is false
int cgcompare_and_jump(int ASTop, int r1, int r2, int label);

// Generates a jump to a given label
void cgjump(int);
// Generates a label, e.g. L1 
void cglabel(int l);
