#include "data.h"
#include "gen.h"
#include "misc.h"
#include "sym.h"

struct symtable *Globhead, *Globtail;
struct symtable *Loclhead, *Locltail;
struct symtable *Parmhead, *Parmtail;
struct symtable *Membhead, *Membtail;
struct symtable *Structhead, *Structtail;
struct symtable *Unionhead, *Uniontail;
struct symtable *Enumhead, *Enumtail;
struct symtable *Typehead, *Typetail;

void appendsym(struct symtable **head, struct symtable **tail, struct symtable *node) {
  if (head == NULL || tail == NULL || node == NULL)
    fatal("Either head, tail or node is NULL in appendsym");

  if (*tail) {
    // Append to the end of the list
    (*tail)->next = node;
    *tail = node;
  } else {
    // Insert first element into the list
    *head = *tail = node;
  }

  node->next = NULL;
}

struct symtable *newsym(char *name, int type, struct symtable *ctype,
    int stype, int class, int size, int posn) {
  struct symtable *node = (struct symtable *) malloc(sizeof(struct symtable));
  if (node == NULL)
    fatal("Unable to malloc a symtable node in newsym");

  node->name = strdup(name);
  node->type = type;
  node->ctype = ctype;
  node->stype = stype;
  node->class = class;
  node->size = size;
  node->posn = posn;
  node->next = NULL;
  node->member = NULL;

  if (class == C_GLOBAL)
    genglobsym(node);

  return node;
}

struct symtable *addglob(char *name, int type, struct symtable *ctype, 
    int stype, int class, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, class, size, 0);
  appendsym(&Globhead, &Globtail, sym);
  return sym;
}

struct symtable *addlocl(char *name, int type, struct symtable *ctype, 
    int stype, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_LOCAL, size, 0);
  appendsym(&Loclhead, &Locltail, sym);
  return sym;
}

struct symtable *addparm(char *name, int type, struct symtable *ctype, int stype, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_PARAM, size, 0);
  appendsym(&Parmhead, &Parmtail, sym);
  return sym;
}

struct symtable *addmemb(char *name, int type, struct symtable *ctype,
			 int stype, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_MEMBER, size, 0);
  appendsym(&Membhead, &Membtail, sym);
  return sym;
}

struct symtable *addunion(char *name, int type, struct symtable *ctype,
			   int stype, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_UNION, size, 0);
  appendsym(&Unionhead, &Uniontail, sym);
  return sym;
}

struct symtable *addstruct(char *name, int type, struct symtable *ctype,
			   int stype, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_STRUCT, size, 0);
  appendsym(&Structhead, &Structtail, sym);
  return sym;
}

// Add an enum type or value to the enum list.
// Class is C_ENUMTYPE or C_ENUMVAL.
// Use posn to store the int value.
struct symtable *addenum(char *name, int class, int value) {
  struct symtable *sym = newsym(name, P_INT, NULL, 0, class, 0, value);
  appendsym(&Enumhead, &Enumtail, sym);
  return sym;
}

// Add a typedef to the typedef list
struct symtable *addtypedef(char *name, int type, struct symtable *ctype,
			   int stype, int size) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_TYPEDEF, size, 0);
  appendsym(&Typehead, &Typetail, sym);
  return sym;
}

// Find a node with a matching name from the list. If the class given is not 0,
// also match the node's class with the given class.
static struct symtable *findsyminlist(char *s, struct symtable *list, int class) {
  for (; list != NULL; list = list->next) {
    if ((list->name != NULL) && !strcmp(s, list->name))
      if (class == 0 || class == list->class)
        return list;
  }

  return NULL;
}

struct symtable *findglob(char *s) {
  return findsyminlist(s, Globhead, 0);
}

struct symtable *findlocl(char *s) {
  struct symtable *node;

  // Look for a parameter if we are in a function's body
  if (Functionid) {
    node = findsyminlist(s, Functionid->member, 0);
    if (node)
      return node;
  }

  return findsyminlist(s, Loclhead, 0);
}

struct symtable *findsymbol(char *s) {
  struct symtable *node;

  // First search params of a function if
  // we are within one
  if (Functionid) {
    node = findsyminlist(s, Functionid->member, 0);
    if (node) return node;
  }

  // Then try the local list
  node = findsyminlist(s, Loclhead, 0);
  if (node) return node;

  // And finally the global list
  return findsyminlist(s, Globhead, 0);
}

struct symtable *findunion(char *s) {
  return findsyminlist(s, Unionhead, 0);
}

struct symtable *findstruct(char *s) {
  return findsyminlist(s, Structhead, 0);
}

// Find an enum type in the enum list
// Return a pointer to the found node or NULL if not found.
struct symtable *findenumtype(char *s) {
  return (findsyminlist(s, Enumhead, C_ENUMTYPE));
}

// Find an enum value in the enum list
// Return a pointer to the found node or NULL if not found.
struct symtable *findenumval(char *s) {
  return (findsyminlist(s, Enumhead, C_ENUMVAL));
}

// Find a type in the tyedef list
// Return a pointer to the found node or NULL if not found.
struct symtable *findtypedef(char *s) {
  return (findsyminlist(s, Typehead, 0));
}

void clear_symtable(void) {
  Globhead = Globtail = NULL;
  Loclhead = Locltail = NULL;
  Parmhead = Parmtail = NULL;
  Membhead = Membtail = NULL;
  Structhead = Structtail = NULL;
}

void freeloclsyms(void) {
  Loclhead = Locltail = NULL;
  Parmhead = Parmtail = NULL;
  Functionid = NULL;
}

