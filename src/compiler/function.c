#include "function.h"
#include "bytecode.h"

userfunction *New_Userfunction(Bytecode *c, Varlist *v, char *name) {
    userfunction *u = malloc(sizeof(userfunction));

    u->code = c->code;
    u->codesize = c->len;

    u->labels = set_label_opcode(c);
    u->nlvars = v->vars->len;
    u->var_info = v;
    u->name = name;

    return u;
}

