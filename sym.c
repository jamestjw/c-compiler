#include "data.h"
#include "gen.h"
#include "misc.h"
#include "sym.h"
#include "types.h"

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
    int stype, int class, int nelems, int posn) {
  struct symtable *node = (struct symtable *) malloc(sizeof(struct symtable));
  if (node == NULL)
    fatal("Unable to malloc a symtable node in newsym");

  if (name == NULL)
    node->name = NULL;
  else
    node->name = strdup(name);
  node->type = type;
  node->ctype = ctype;
  node->stype = stype;
  node->class = class;
  node->nelems = nelems;

  // For pointers and integer types, set the size of the symbol.
  // Structs and unions declarations set this up themselves.
  if (ptrtype(type) || inttype(type))
    node->size = nelems * typesize(type, ctype);

  node->st_posn = posn;
  node->next = NULL;
  node->member = NULL;
  node->initlist = NULL;

  return node;
}

struct symtable *addglob(char *name, int type, struct symtable *ctype,
    int stype, int class, int nelems, int posn) {
  struct symtable *sym = newsym(name, type, ctype, stype, class, nelems, posn);

  if (type == P_STRUCT || type == P_UNION)
    sym->size = ctype->size;

  appendsym(&Globhead, &Globtail, sym);
  return sym;
}

struct symtable *addlocl(char *name, int type, struct symtable *ctype,
    int stype, int nelems) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_LOCAL, nelems, 0);

  if (type == P_STRUCT || type == P_UNION)
    sym->size = ctype->size;

  appendsym(&Loclhead, &Locltail, sym);
  return sym;
}

struct symtable *addparm(char *name, int type, struct symtable *ctype, int stype) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_PARAM, 1, 0);
  appendsym(&Parmhead, &Parmtail, sym);
  return sym;
}

struct symtable *addmemb(char *name, int type, struct symtable *ctype,
			 int stype, int nelems) {
  struct symtable *sym = newsym(name, type, ctype, stype, C_MEMBER, nelems, 0);
  appendsym(&Membhead, &Membtail, sym);
  return sym;
}

struct symtable *addunion(char *name) {
  struct symtable *sym = newsym(name, P_UNION, NULL, 0, C_UNION, 0, 0);
  appendsym(&Unionhead, &Uniontail, sym);
  return sym;
}

struct symtable *addstruct(char *name) {
  struct symtable *sym = newsym(name, P_STRUCT, NULL, 0, C_STRUCT, 0, 0);
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
struct symtable *addtypedef(char *name, int type, struct symtable *ctype) {
  struct symtable *sym = newsym(name, type, ctype, 0, C_TYPEDEF, 0, 0);
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

// Find a member in the member list
// Return a pointer to the found node or NULL if not found.
struct symtable *findmember(char *s) {
  return (findsyminlist(s, Membhead, 0));
}

void clear_symtable(void) {
  Globhead = Globtail = NULL;
  Loclhead = Locltail = NULL;
  Parmhead = Parmtail = NULL;
  Membhead = Membtail = NULL;
  Structhead = Structtail = NULL;
  Unionhead = Uniontail = NULL;
  Enumhead = Enumtail = NULL;
  Typehead = Typetail = NULL;
}

void freeloclsyms(void) {
  Loclhead = Locltail = NULL;
  Parmhead = Parmtail = NULL;
  Functionid = NULL;
}

// Remove all static symbols from the global symbol table
void freestaticsyms(void) {
  struct symtable *g, *prev = NULL;

  for (g = Globhead; g != NULL; g = g->next) {
    if (g->class == C_STATIC) {
      if (prev != NULL) prev->next = g->next;
      else Globhead->next = g->next;

      if (g == Globtail) {
        if (prev != NULL) Globtail = prev;
        else Globtail = Globhead;
      }
    }
    prev = g;
  }
}

void dumptable(struct symtable *head, char *name, int indent);

static void dumpsym(struct symtable *sym, int indent) {
  int i;

  for (i = 0; i < indent; i++)
    printf(" ");
  switch (sym->type & (~0xf)) {
  case P_VOID:
    printf("void ");
    break;
  case P_CHAR:
    printf("char ");
    break;
  case P_INT:
    printf("int ");
    break;
  case P_LONG:
    printf("long ");
    break;
  case P_STRUCT:
    if (sym->ctype != NULL)
      printf("struct %s ", sym->ctype->name);
    else
      printf("struct %s ", sym->name);
    break;
  case P_UNION:
    if (sym->ctype != NULL)
      printf("union %s ", sym->ctype->name);
    else
      printf("union %s ", sym->name);
    break;
  default:
    printf("unknown type ");
  }

  for (i = 0; i < (sym->type & 0xf); i++)
    printf("*");
  printf("%s", sym->name);

  switch (sym->stype) {
  case S_VARIABLE:
    break;
  case S_FUNCTION:
    printf("()");
    break;
  case S_ARRAY:
    printf("[]");
    break;
  default:
    printf(" unknown stype");
  }

  switch (sym->class) {
  case C_GLOBAL:
    printf(": global");
    break;
  case C_LOCAL:
    printf(": local");
    break;
  case C_PARAM:
    printf(": param");
    break;
  case C_EXTERN:
    printf(": extern");
    break;
  case C_STATIC:
    printf(": static");
    break;
  case C_STRUCT:
    printf(": struct");
    break;
  case C_UNION:
    printf(": union");
    break;
  case C_MEMBER:
    printf(": member");
    break;
  case C_ENUMTYPE:
    printf(": enumtype");
    break;
  case C_ENUMVAL:
    printf(": enumval");
    break;
  case C_TYPEDEF:
    printf(": typedef");
    break;
  default:
    printf(": unknown class");
  }

  switch (sym->stype) {
  case S_VARIABLE:
    if (sym->class == C_ENUMVAL)
      printf(", value %d\n", sym->st_posn);
    else
      printf(", size %d\n", sym->size);
    break;
  case S_FUNCTION:
    printf(", %d params\n", sym->nelems);
    break;
  case S_ARRAY:
    printf(", %d elems, size %d\n", sym->nelems, sym->size);
    break;
  }

  switch (sym->type & (~0xf)) {
  case P_STRUCT:
  case P_UNION:
    dumptable(sym->member, NULL, 4);
  }

  switch (sym->stype) {
  case S_FUNCTION:
    dumptable(sym->member, NULL, 4);
  }
}

void dumptable(struct symtable *head, char *name, int indent) {
  struct symtable *sym;

  if (head != NULL && name != NULL)
    printf("%s\n--------\n", name);

  for (sym = head; sym != NULL; sym = sym->next)
    dumpsym(sym, indent);
}

void dumpsymtables(void) {
  dumptable(Globhead, "Global", 0);
  printf("\n");
  dumptable(Enumhead, "Enums", 0);
  printf("\n");
  dumptable(Typehead, "Typedefs", 0);
}
