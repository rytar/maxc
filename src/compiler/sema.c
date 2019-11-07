#include "sema.h"
#include "error.h"
#include "maxc.h"
#include "struct.h"
#include "lexer.h"
#include "parser.h"

static Ast *visit(Ast *);
static void setup_bltin(void);
static Type *set_bltinfn_type(enum BLTINFN, Type *);

static Ast *visit_binary(Ast *);
static Ast *visit_unary(Ast *);
static Ast *visit_assign(Ast *);
static Ast *visit_member(Ast *);
static Ast *visit_subscr(Ast *);
static Ast *visit_object(Ast *);
static Ast *visit_struct_init(Ast *);
static Ast *visit_block(Ast *);
static Ast *visit_typed_block(Ast *);
static Ast *visit_nonscope_block(Ast *);
static Ast *visit_list(Ast *);
static Ast *visit_if(Ast *);
static Ast *visit_for(Ast *);
static Ast *visit_exprif(Ast *);
static Ast *visit_while(Ast *);
static Ast *visit_return(Ast *);
static Ast *visit_vardecl(Ast *);
static Ast *visit_load(Ast *);
static Ast *visit_funcdef(Ast *);
static Ast *visit_fncall(Ast *);
static Ast *visit_break(Ast *);
static Ast *visit_bltinfn_call(NodeFnCall *, Vector *);

static NodeVariable *do_variable_determining(char *);
static NodeVariable *determining_overload(NodeVariable *, Vector *);
static Type *solve_undefined_type(Type *);
static Type *checktype(Type *, Type *);
static Type *checktype_optional(Type *, Type *);

static Scope scope;
static FuncEnv fnenv;
static Vector *fn_saver;
static int loop_nest = 0;

int ngvar = 0;

int sema_analysis(Vector *ast) {
    scope.current = New_Env_Global();
    fnenv.current = New_Env_Global();
    fn_saver = New_Vector();

    setup_bltin();

    for(int i = 0; i < ast->len; ++i) {
        ast->data[i] = visit((Ast *)ast->data[i]);
    }

    ngvar += fnenv.current->vars->vars->len;

    var_set_number(fnenv.current->vars);

    scope_escape(&scope);

    return ngvar;
}

static void setup_bltin() {
    char *bltfns_name[] = {
        "print",
        "println",
        "objectid",
        "len",
        "tofloat",
        "error",
    };
    enum BLTINFN bltfns_kind[] = {
        BLTINFN_PRINT,
        BLTINFN_PRINTLN,
        BLTINFN_OBJECTID,
        BLTINFN_STRINGSIZE,
        BLTINFN_INTTOFLOAT,
        BLTINFN_ERROR,
    };

    int nfn = sizeof(bltfns_kind) / sizeof(bltfns_kind[0]);

    Varlist *bltfns = New_Varlist();

    for(int i = 0; i < nfn; ++i) {
        Type *fntype = New_Type(CTYPE_FUNCTION);

        fntype = set_bltinfn_type(bltfns_kind[i], fntype);

        func_t finfo = New_Func_t_With_Bltin(bltfns_kind[i], fntype);

        NodeVariable *a = new_node_variable_with_func(bltfns_name[i], finfo);
        a->isglobal = true;
        a->isbuiltin = true;

        varlist_push(bltfns, a);
    }

    varlist_mulpush(fnenv.current->vars, bltfns);
    varlist_mulpush(scope.current->vars, bltfns);
}

static Type *set_bltinfn_type(enum BLTINFN kind, Type *ty) {
    switch(kind) {
    case BLTINFN_PRINT:
    case BLTINFN_PRINTLN:
        ty->fnret = mxcty_none;
        vec_push(ty->fnarg, New_Type(CTYPE_ANY_VARARG));
        break;
    case BLTINFN_OBJECTID:
        ty->fnret = mxcty_int;
        vec_push(ty->fnarg, New_Type(CTYPE_ANY));
        break;
    case BLTINFN_STRINGSIZE:
        ty->fnret = mxcty_int;
        vec_push(ty->fnarg, mxcty_string);
        break;
    case BLTINFN_INTTOFLOAT:
        ty->fnret = mxcty_float;
        vec_push(ty->fnarg, mxcty_int);
        break;
    case BLTINFN_ERROR:
        ty->fnret = New_Type(CTYPE_ERROR);
        vec_push(ty->fnarg, mxcty_string);
        break;
    default:
        mxc_assert(0, "maxc internal error");
    }

    return ty;
}

static Ast *visit(Ast *ast) {
    if(ast == NULL)
        return NULL;

    switch(ast->type) {
    case NDTYPE_NUM:
    case NDTYPE_BOOL:
    case NDTYPE_CHAR:
    case NDTYPE_STRING:
        break;
    case NDTYPE_LIST:
        return visit_list(ast);
    case NDTYPE_SUBSCR:
        return visit_subscr(ast);
    case NDTYPE_TUPLE:
        mxc_unimplemented("tuple");
        return ast;
    case NDTYPE_OBJECT:
        return visit_object(ast);
    case NDTYPE_STRUCTINIT:
        return visit_struct_init(ast);
    case NDTYPE_BINARY:
        return visit_binary(ast);
    case NDTYPE_MEMBER:
        return visit_member(ast);
    case NDTYPE_UNARY:
        return visit_unary(ast);
    case NDTYPE_ASSIGNMENT:
        return visit_assign(ast);
    case NDTYPE_IF:
        return visit_if(ast);
    case NDTYPE_EXPRIF:
        return visit_exprif(ast);
    case NDTYPE_FOR:
        return visit_for(ast);
    case NDTYPE_WHILE:
        return visit_while(ast);
    case NDTYPE_BLOCK:
        return visit_block(ast);
    case NDTYPE_TYPEDBLOCK:
        return visit_typed_block(ast);
    case NDTYPE_NONSCOPE_BLOCK:
        return visit_nonscope_block(ast);
    case NDTYPE_RETURN:
        return visit_return(ast);
    case NDTYPE_BREAK:
        return visit_break(ast);
    case NDTYPE_VARIABLE:
        return visit_load(ast);
    case NDTYPE_FUNCCALL:
        return visit_fncall(ast);
    case NDTYPE_FUNCDEF:
        return visit_funcdef(ast);
    case NDTYPE_VARDECL:
        return visit_vardecl(ast);
    default:
        mxc_assert(0, "internal error");
    }

    return ast;
}

static Ast *visit_list(Ast *ast) {
    NodeList *l = (NodeList *)ast;

    Type *base = NULL;
    if(l->nsize != 0) {
        base = CAST_AST(l->elem->data[0])->ctype;

        for(int i = 0; i < l->nsize; ++i) {
            l->elem->data[i] = visit(l->elem->data[i]);
            checktype(base, CAST_AST(l->elem->data[i])->ctype);
        }
    }

    CAST_AST(l)->ctype = New_Type_With_Ptr(base);

    return (Ast *)l;
}

static Ast *visit_binary(Ast *ast) {
    NodeBinop *b = (NodeBinop *)ast;

    b->left = visit(b->left);
    b->right = visit(b->right);

    MxcOp *res = check_op_definition(OPE_BINARY, b->op, b->left->ctype, b->right->ctype);

    if(res == NULL) {
        error("undefined operation `%s` between %s and %s",
                operator_dump(b->op),
                typedump(b->left->ctype),
                typedump(b->right->ctype)
             );

        goto err;
    }

    CAST_AST(b)->ctype = res->ret;

    if(res->impl != NULL) {
        Vector *arg = New_Vector_With_Size(2);

        arg->data[0] = b->left;
        arg->data[1] = b->right;

        res->call = new_node_fncall((Ast *)res->impl->fnvar, arg, NULL);
        b->impl = res->call;
    }

err:
    return CAST_AST(b);
}

static Ast *visit_unary(Ast *ast) {
    NodeUnaop *u = (NodeUnaop *)ast;

    u->expr = visit(u->expr);

    CAST_AST(u)->ctype = u->expr->ctype;

    return CAST_AST(u);
}

static Ast *visit_assign(Ast *ast) {
    NodeAssignment *a = (NodeAssignment *)ast;

    if(a->dst->type != NDTYPE_VARIABLE && a->dst->type != NDTYPE_SUBSCR &&
       a->dst->type != NDTYPE_MEMBER) {
        error("left side of the expression is not valid");
    }

    a->dst = visit(a->dst);

    NodeVariable *v = (NodeVariable *)a->dst;
    // TODO: subscr?

    if(v->vinfo.vattr & (int)VARATTR_CONST) {
        error("assignment of read-only variable: %s", v->name);
    }

    a->src = visit(a->src);

    v->vinfo.vattr &= ~((int)VARATTR_UNINIT);

    checktype(a->dst->ctype, a->src->ctype);

    return CAST_AST(a);
}

static Ast *visit_subscr(Ast *ast) {
    NodeSubscript *s = (NodeSubscript *)ast;

    s->ls = visit(s->ls);
    s->index = visit(s->index);

    CAST_AST(s)->ctype = CAST_AST(s->ls)->ctype->ptr;

    return (Ast *)s;
}

static Ast *visit_member(Ast *ast) {
    NodeMember *m = (NodeMember *)ast;

    m->left = visit(m->left);

    if(type_is(m->left->ctype, CTYPE_LIST)) {
        NodeVariable *rhs = (NodeVariable *)m->right;

        if(strcmp(rhs->name, "len") == 0) {
            CAST_AST(m)->ctype = mxcty_int;

            goto success;
        }
    }

    if(m->right->type == NDTYPE_VARIABLE) {
        // field
        NodeVariable *rhs = (NodeVariable *)m->right;
        size_t nfield = m->left->ctype->strct.nfield;

        for(size_t i = 0; i < nfield; ++i) {
            if(strncmp(m->left->ctype->strct.field[i]->name,
                       rhs->name,
                       strlen(m->left->ctype->strct.field[i]->name)) == 0) {
                CAST_AST(m)->ctype =
                    CAST_AST(m->left->ctype->strct.field[i])->ctype;
                goto success;
            }
        }
        error("No field: %s", rhs->name);
    }
    else {
        m->right = visit(m->right);
    }

success:
    return CAST_AST(m);
}

static Ast *visit_object(Ast *ast) {
    NodeObject *s = (NodeObject *)ast;

    mxc_assert(CAST_AST(s->decls->data[0])->type == NDTYPE_VARIABLE,
               "internal error");

    MxcStruct struct_info = New_MxcStruct(
        s->tagname, (NodeVariable **)s->decls->data, s->decls->len);

    vec_push(scope.current->userdef_type, New_Type_With_Struct(struct_info));

    return CAST_AST(s);
}

static Ast *visit_struct_init(Ast *ast) {
    NodeStructInit *s = (NodeStructInit *)ast;

    s->tag = solve_undefined_type(s->tag);
    CAST_AST(s)->ctype = s->tag;

    for(int i = 0; i < s->fields->len; ++i) {
        // TODO
        ;
    }
    for(int i = 0; i < s->inits->len; ++i) {
        s->inits->data[i] = visit(s->inits->data[i]);
    }

    return CAST_AST(s);
}

static Ast *visit_block(Ast *ast) {
    NodeBlock *b = (NodeBlock *)ast;

    scope_make(&scope);

    for(int i = 0; i < b->cont->len; ++i) {
        b->cont->data[i] = visit(b->cont->data[i]);
    }

    scope_escape(&scope);

    return CAST_AST(b);
}

static Ast *visit_typed_block(Ast *ast) {
    NodeBlock *b = (NodeBlock *)ast;

    scope_make(&scope);

    for(int i = 0; i < b->cont->len; ++i) {
        b->cont->data[i] = visit(b->cont->data[i]);
    }

    scope_escape(&scope);

    CAST_AST(b)->ctype = ((Ast *)b->cont->data[b->cont->len - 1])->ctype;

    return CAST_AST(b);
}

static Ast *visit_nonscope_block(Ast *ast) {
    NodeBlock *b = (NodeBlock *)ast;

    for(int i = 0; i < b->cont->len; ++i) {
        b->cont->data[i] = visit(b->cont->data[i]);
    }

    return CAST_AST(b);
}

static Ast *visit_if(Ast *ast) {
    NodeIf *i = (NodeIf *)ast;

    i->cond = visit(i->cond);
    i->then_s = visit(i->then_s);
    i->else_s = visit(i->else_s);

    CAST_AST(i)->ctype = mxcty_none;

    return CAST_AST(i);
}

static Ast *visit_exprif(Ast *ast) {
    NodeIf *i = (NodeIf *)ast;

    i->cond = visit(i->cond);
    i->then_s = visit(i->then_s);
    i->else_s = visit(i->else_s);

    CAST_AST(i)->ctype = checktype(i->then_s->ctype, i->else_s->ctype);

    return CAST_AST(i);
}

static Ast *visit_for(Ast *ast) {
    NodeFor *f = (NodeFor *)ast;

    f->init = visit(f->init);

    f->cond = visit(f->cond);
    if(checktype(f->cond->ctype, mxcty_bool) == NULL) {
        error("for error");
    }

    f->reinit = visit(f->reinit);

    f->body = visit(f->body);

    return CAST_AST(f);
}

static Ast *visit_while(Ast *ast) {
    NodeWhile *w = (NodeWhile *)ast;

    w->cond = visit(w->cond);

    loop_nest++;
    w->body = visit(w->body);
    loop_nest--;

    return CAST_AST(w);
}

static Ast *visit_return(Ast *ast) {
    NodeReturn *r = (NodeReturn *)ast;

    r->cont = visit(r->cont);

    if(fn_saver->len == 0) {
        error("use of return statement outside function or block");
    }
    else {
        Type *cur_fn_retty =
            ((NodeFunction *)vec_last(fn_saver))->finfo.ftype->fnret;

        if(!checktype(cur_fn_retty, r->cont->ctype)) {
            if(type_is(cur_fn_retty, CTYPE_OPTIONAL)) {
                if(!type_is(r->cont->ctype, CTYPE_ERROR)) {
                    error("return type error: expected error, found %s",
                            typedump(r->cont->ctype));
                }
            }
            else {
                error("type error: expected %s, found %s",
                        typedump(cur_fn_retty),
                        typedump(r->cont->ctype));
            }
        }
    }

    return CAST_AST(r);
}

static Ast *visit_break(Ast *ast) {
    NodeBreak *b = (NodeBreak *)ast;

    if(loop_nest == 0) {
        error("break statement must be inside loop statement");
    }

    return (Ast *)b;
}

static Ast *visit_vardecl(Ast *ast) {
    NodeVardecl *v = (NodeVardecl *)ast;

    v->var->isglobal = funcenv_isglobal(fnenv);

    if(v->init != NULL) {
        v->init = visit(v->init);

        if(type_is(CAST_AST(v->var)->ctype, CTYPE_UNINFERRED)) {
            CAST_AST(v->var)->ctype = v->init->ctype;
        }
        else if(!checktype(CAST_AST(v->var)->ctype, v->init->ctype)) {
            error(
                "`%s` type is %s",
                v->var->name,
                typedump(CAST_AST(v->var)->ctype)
            );
        }
    }
    else {
        if(type_is(CAST_AST(v->var)->ctype, CTYPE_UNINFERRED)) {
            error("Must always be initialized when doing type inference.");
        }
        else if(type_is(CAST_AST(v->var)->ctype, CTYPE_UNDEFINED)) {
            CAST_AST(v->var)->ctype =
                solve_undefined_type(CAST_AST(v->var)->ctype);
        }

        v->var->vinfo.vattr |= (int)VARATTR_UNINIT;
    }

    varlist_push(fnenv.current->vars, v->var);
    varlist_push(scope.current->vars, v->var);

    return CAST_AST(v);
}

static Ast *visit_fncall(Ast *ast) {
    NodeFnCall *f = (NodeFnCall *)ast;

    Vector *argtys = New_Vector();

    for(int i = 0; i < f->args->len; ++i) {
        f->args->data[i] = visit(f->args->data[i]);
        vec_push(argtys, CAST_AST(f->args->data[i])->ctype);
    }

    f->func = visit(f->func);

    if(f->func->type == NDTYPE_VARIABLE) {
        f->func = (Ast *)determining_overload((NodeVariable *)f->func, argtys);
    }
    else {
        mxc_unimplemented("error");
        // TODO
    }

    if(((NodeVariable *)f->func)->finfo.isbuiltin) {
        return visit_bltinfn_call(f, argtys);
    }

    NodeVariable *fn = (NodeVariable *)f->func;

    CAST_AST(f)->ctype = fn->finfo.ftype->fnret;

    if(f->failure_block) {
        if(!type_is(((Ast *)f)->ctype, CTYPE_OPTIONAL)) {
            error("Use of failure blocks other than optional types is prohibited");
        }
        else {
            // TODO
            f->failure_block = visit(f->failure_block);

            if(!checktype(((MxcOptional *)((Ast *)f)->ctype)->base,
                            f->failure_block->ctype)) {
                error("type error: failure_block");
            }

            CAST_AST(f)->ctype = ((MxcOptional *)CAST_AST(f)->ctype)->base;
        }
    }

    return (Ast *)f;
}

static Ast *visit_funcdef(Ast *ast) {
    NodeFunction *fn = (NodeFunction *)ast;

    vec_push(fn_saver, fn);

    fn->fnvar->isglobal = funcenv_isglobal(fnenv);

    varlist_push(fnenv.current->vars, fn->fnvar);
    varlist_push(scope.current->vars, fn->fnvar);

    funcenv_make(&fnenv);
    scope_make(&scope);

    fn->fnvar->vinfo.vattr = 0;

    for(int i = 0; i < fn->finfo.args->vars->len; ++i) {
        ((NodeVariable *)fn->finfo.args->vars->data[i])->isglobal = false;
        if(type_is(CAST_AST(fn->fnvar)->ctype->fnarg->data[i],
                   CTYPE_UNDEFINED)) {
            CAST_AST(fn->fnvar)->ctype->fnarg->data[i] =
                solve_undefined_type(
                    CAST_AST(fn->fnvar)->ctype->fnarg->data[i]
                );
        }

        varlist_push(fnenv.current->vars, fn->finfo.args->vars->data[i]);
        varlist_push(scope.current->vars, fn->finfo.args->vars->data[i]);
    }

    fn->block = visit(fn->block);

    if(fn->block->type != NDTYPE_BLOCK) {
        // expr
        if(type_is(fn->finfo.ftype->fnret, CTYPE_UNINFERRED)) {
            fn->finfo.ftype->fnret = fn->block->ctype;
        }
        else {
            if(type_is(fn->finfo.ftype->fnret, CTYPE_UNDEFINED)) {
                fn->finfo.ftype->fnret =
                    solve_undefined_type(fn->finfo.ftype->fnret);
            }

            Type *suc = checktype(fn->finfo.ftype->fnret, fn->block->ctype);

            if(!suc) {
                error("return type error");
            }
        }
    }

    var_set_number(fnenv.current->vars);

    fn->lvars = fnenv.current->vars;

    if(fn->op != -1) {
        New_Op(
            OPE_BINARY,
            fn->op,
            ((Type *)((Ast *)fn->fnvar)->ctype->fnarg->data[0]),
            ((Type *)((Ast *)fn->fnvar)->ctype->fnarg->data[1]),
            fn->finfo.ftype->fnret,
            fn
        );
    }

    funcenv_escape(&fnenv);
    scope_escape(&scope);

    vec_pop(fn_saver);

    return CAST_AST(fn);
}

static bool print_arg_check(Vector *argtys) {
    for(int i = 0; i < argtys->len; i++) {
        if(!(((Type *)argtys->data[i])->impl & TIMPL_SHOW)) {
            error(
                "type %s does not implement `Show`",
                typedump(((Type *)argtys->data[i]))
            );

            return false;
        }
    }

    return true;
}

static Ast *visit_bltinfn_call(NodeFnCall *f, Vector *argtys) {
    f->func = visit(f->func);

    NodeVariable *fn = (NodeVariable *)f->func;

    if(fn == NULL)
        return NULL;

    if(strncmp(fn->name, "print",   5) == 0 ||
       strncmp(fn->name, "println", 7) == 0) {
        print_arg_check(argtys);
    }

    CAST_AST(f)->ctype = CAST_AST(fn)->ctype->fnret;

    return CAST_AST(f);
}

static Ast *visit_load(Ast *ast) {
    NodeVariable *v = (NodeVariable *)ast;

    v = do_variable_determining(v->name);

    if(type_is(CAST_AST(v)->ctype, CTYPE_UNDEFINED)) {
        CAST_AST(v)->ctype = solve_undefined_type(CAST_AST(v)->ctype);
    }
    if((v->vinfo.vattr & (int)VARATTR_UNINIT) &&
       !type_is(CAST_AST(v)->ctype, CTYPE_STRUCT)) {
        error("use of uninit variable: %s", v->name);
    }

    v->used = true;

    return CAST_AST(v);
}

static NodeVariable *do_variable_determining(char *name) {
    for(Env *e = scope.current;; e = e->parent) {
        if(!e->vars->vars->len == 0)
            break;
        if(e->isglb) {
            // debug("empty\n");
            goto verr;
        }
    }

    // fnenv.current->vars.show();
    for(Env *e = scope.current;; e = e->parent) {
        for(int i = 0; i < e->vars->vars->len; ++i) {
            if(strcmp(((NodeVariable *)e->vars->vars->data[i])->name, name) ==
               0) {
                return (NodeVariable *)e->vars->vars->data[i];
            }
        }
        if(e->isglb) {
            // debug("it is glooobal\n");
            goto verr;
        }
    }

verr:
    error("undeclared variable: %s", name);
    /*
    error(token.see(-1).line, token.see(-1).col,
            "undeclared variable: `%s`", tk.value.c_str());
    error(tk.start, tk.end, "undeclared variable: `%s`", tk.value.c_str());
    */
    return NULL;
}

static NodeVariable *determining_overload(NodeVariable *var, Vector *argtys) {
    if(var == NULL) {
        return NULL;
    }

    for(Env *e = scope.current;; e = e->parent) {
        for(int i = 0; i < e->vars->vars->len; ++i) {
            NodeVariable *v = (NodeVariable *)e->vars->vars->data[i];
            if(strlen(v->name) != strlen(var->name))
                continue;
            if(strncmp(v->name, var->name, strlen(v->name)) == 0) {
                if(CAST_AST(v)->ctype->fnarg->len == argtys->len &&
                   argtys->len == 0) {
                    return v;
                }
                else if(CAST_AST(v)->ctype->fnarg->len == 0)
                    continue;
                // args size check
                if(CAST_TYPE(CAST_AST(v)->ctype->fnarg->data[0])->type ==
                   CTYPE_ANY_VARARG)
                    return v;
                else if(CAST_TYPE(CAST_AST(v)->ctype->fnarg->data[0])->type ==
                        CTYPE_ANY) {
                    if(argtys->len == 1)
                        return v;
                    else {
                        error("the number of %s() argument must be 1", v->name);
                        return NULL;
                    }
                }

                if(CAST_AST(v)->ctype->fnarg->len == argtys->len) {
                    // type check
                    bool is_same = true;
                    for(int i = 0; i < CAST_AST(v)->ctype->fnarg->len; ++i) {
                        if(CAST_TYPE(CAST_AST(v)->ctype->fnarg->data[i])
                               ->type != CAST_TYPE(argtys->data[i])->type) {
                            is_same = false;
                        }
                    }

                    if(is_same)
                        return v;
                }
            }
        }

        if(e->isglb) {
            // debug("it is glooobal\n");
            goto err;
        }
    }

err:
    error("No Function!: %s(%s)", var->name, typedump((Type *)argtys->data[0]));

    return NULL;
}

static Type *checktype_optional(Type *ty1, Type *ty2) {
    Type *checked = checktype(ty1, ty2);

    if(checked != NULL) {
        return checked;
    }

    if(ty1->optional) {
        if(ty2->type == CTYPE_ERROR) {
            return ty1;
        }
    }

    return NULL;
}

static Type *checktype(Type *ty1, Type *ty2) {
    if(ty1 == NULL || ty2 == NULL)
        return NULL;

    if(type_is(ty1, CTYPE_UNDEFINED))
        ty1 = solve_undefined_type(ty1);
    if(type_is(ty2, CTYPE_UNDEFINED))
        ty2 = solve_undefined_type(ty2);

    if(type_is(ty1, CTYPE_OPTIONAL)) {
        ty1 = ((MxcOptional *)ty1)->base;
    }
    if(type_is(ty2, CTYPE_OPTIONAL)) {
        ty2 = ((MxcOptional *)ty2)->base;
    }

    if(type_is(ty1, CTYPE_LIST)) {
        if(!type_is(ty2, CTYPE_LIST))
            goto err;
        Type *b = ty1;

        for(;;) {
            ty1 = ty1->ptr;
            ty2 = ty2->ptr;

            if(ty1 == NULL && ty2 == NULL)
                return b;
            if(ty1 == NULL || ty2 == NULL)
                goto err;
            checktype(ty1, ty2);
        }
    }
    else if(type_is(ty1, CTYPE_TUPLE)) {
        if(!type_is(ty2, CTYPE_TUPLE))
            goto err;
        if(ty1->tuple->len != ty2->tuple->len)
            goto err;
        int s = ty1->tuple->len;
        int cnt = 0;

        for(;;) {
            checktype(ty1->tuple->data[cnt], ty2->tuple->data[cnt]);
            ++cnt;
            if(cnt == s)
                return ty1;
        }
    }
    else if(type_is(ty1, CTYPE_FUNCTION)) {
        if(!type_is(ty2, CTYPE_FUNCTION))
            goto err;
        if(ty1->fnarg->len != ty2->fnarg->len)
            goto err;
        if(ty1->fnret->type != ty2->fnret->type)
            goto err;

        int i = ty1->fnarg->len;
        int cnt = 0;

        if(i == 0)
            return ty1;

        for(;;) {
            checktype(ty1->fnarg->data[cnt], ty2->fnarg->data[cnt]);
            ++cnt;
            if(cnt == i)
                return ty1;
        }
    }

    if(ty1->type == ty2->type)
        return ty1;
err:
    return NULL;
}

static Type *solve_undefined_type(Type *ty) {
    for(Env *e = scope.current;; e = e->parent) {
        if(!e->userdef_type->len == 0)
            break;
        if(e->isglb) {
            //debug("empty\n");
            goto err;
        }
    }

    for(Env *e = scope.current;; e = e->parent) {
        for(int i = 0; i < e->userdef_type->len; ++i) {
            if(strcmp(((Type *)e->userdef_type->data[i])->strct.name,
                      ty->name) == 0) {
                return CAST_TYPE(e->userdef_type->data[i]);
            }
        }
        if(e->isglb) {
            goto err;
        }
    }

err:
    error("undefined type: %s", ty->name);
    /*
    error(token.see(-1).line, token.see(-1).col,
            "undeclared variable: `%s`", tk.value.c_str());
    error(tk.start, tk.end, "undeclared variable: `%s`", tk.value.c_str());
    */
    return ty;
}