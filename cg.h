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
// Define a global symbol
void cgglobsym(char *sym);
void cgprintint(int r);
