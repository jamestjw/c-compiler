void match(int t, char *what);
void semi(void);
void lbrace(void);
void rbrace(void);
void lparen(void);
void rparen(void);
void ident(void);
// Prints an error message and terminates the program with exit code 1
void fatal(char *s);
void fatalc(char *s, int c);
void fatald(char *s, int d);
void fatals(char *s1, char *s2);
