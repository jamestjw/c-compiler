void freeall_registers(void);
void cgpreamble();
void cgpostamble();
int cgloadint(int value);
int cgadd(int r1, int r2);
int cgsub(int r1, int r2);
int cgmul(int r1, int r2);
int cgdiv(int r1, int r2);
// Load a value from a global symbol
int cgloadglob(char *identifier);
// Load a value to a global symbol
int cgstorglob(int r, char *identifier);

// Compare values in two registers, sets the
// register that contains the result to 1
// if the comparison is true and 0 otherwise.
int cgequal(int r1, int r2);
int cgnotequal(int r1, int r2);
int cglessthan(int r1, int r2);
int cggreaterthan(int r1, int r2);
int cglessequal(int r1, int r2);
int cggreaterequal(int r1, int r2);

// Define a global symbol
void cgglobsym(char *sym);
void cgprintint(int r);
