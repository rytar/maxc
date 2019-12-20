#include "codegen.h"
#include "bytecode.h"
#include "error.h"
#include "maxc.h"

static void gen(Ast *, Bytecode *iseq, bool);

static void emit_num(Ast *, Bytecode *, bool);
static void emit_bool(Ast *, Bytecode *, bool);
static void emit_char(Ast *, Bytecode *, bool);
static void emit_string(Ast *, Bytecode *, bool);
static void emit_list(Ast *, Bytecode *);
static void emit_listaccess(Ast *, Bytecode *);
static void emit_tuple(Ast *, Bytecode *);
static void emit_binop(Ast *, Bytecode *, bool);
static void emit_member(Ast *, Bytecode *, bool);
static void emit_unaop(Ast *, Bytecode *, bool);
static void emit_if(Ast *, Bytecode *);
static void emit_for(Ast *, Bytecode *);
static void emit_while(Ast *, Bytecode *);
static void emit_return(Ast *, Bytecode *);
static void emit_break(Ast *, Bytecode *);
static void emit_block(Ast *, Bytecode *);
static void emit_typed_block(Ast *, Bytecode *);
static void emit_assign(Ast *, Bytecode *);
static void emit_struct_init(Ast *, Bytecode *, bool);
static void emit_store(Ast *, Bytecode *);
static void emit_member_store(Ast *, Bytecode *);
static void emit_listaccess_store(Ast *, Bytecode *);
static void emit_func_def(Ast *, Bytecode *);
static void emit_func_call(Ast *, Bytecode *, bool);
static void emit_bltinfunc_call(NodeFnCall *, Bytecode *, bool);
static void emit_bltinfncall_print(NodeFnCall *, Bytecode *, bool);
static void emit_bltinfncall_println(NodeFnCall *, Bytecode *, bool);
static void emit_vardecl(Ast *, Bytecode *);
static void emit_load(Ast *, Bytecode *, bool);

static int show_from_type(enum CTYPE);

Vector *ltable;
Vector *loop_stack;

static void compiler_init() {
    ltable = New_Vector();
    loop_stack = New_Vector();
}

Bytecode *compile(Vector *ast) {
    Bytecode *iseq = New_Bytecode();
    compiler_init();

    for(int i = 0; i < ast->len; ++i) {
        gen((Ast *)ast->data[i], iseq, false);
    }

    push_0arg(iseq, OP_END);

    return iseq;
}

Bytecode *compile_repl(Vector *ast) {
    Bytecode *iseq = New_Bytecode();
    compiler_init();

    for(int i = 0; i < ast->len; ++i) {
        gen((Ast *)ast->data[i], iseq, true);
    }

    push_0arg(iseq, OP_END);

    return iseq;
}

static void gen(Ast *ast, Bytecode *iseq, bool use_ret) {
    if(ast == NULL) {
        return;
    }

    switch(ast->type) {
    case NDTYPE_NUM:
        emit_num(ast, iseq, use_ret);
        break;
    case NDTYPE_BOOL:
        emit_bool(ast, iseq, use_ret);
        break;
    case NDTYPE_CHAR:
        emit_char(ast, iseq, use_ret);
        break;
    case NDTYPE_STRING:
        emit_string(ast, iseq, use_ret);
        break;
    case NDTYPE_OBJECT:
        break;
    case NDTYPE_STRUCTINIT:
        emit_struct_init(ast, iseq, use_ret);
        break;
    case NDTYPE_LIST:
        emit_list(ast, iseq);
        break;
    case NDTYPE_SUBSCR:
        emit_listaccess(ast, iseq);
        break;
    case NDTYPE_TUPLE:
        emit_tuple(ast, iseq);
        break;
    case NDTYPE_BINARY:
        emit_binop(ast, iseq, use_ret);
        break;
    case NDTYPE_MEMBER:
        emit_member(ast, iseq, use_ret);
        break;
    case NDTYPE_UNARY:
        emit_unaop(ast, iseq, use_ret);
        break;
    case NDTYPE_ASSIGNMENT:
        emit_assign(ast, iseq);
        break;
    case NDTYPE_IF:
    case NDTYPE_EXPRIF:
        emit_if(ast, iseq);
        break;
    case NDTYPE_FOR:
        emit_for(ast, iseq);
        break;
    case NDTYPE_WHILE:
        emit_while(ast, iseq);
        break;
    case NDTYPE_BLOCK:
    case NDTYPE_NONSCOPE_BLOCK:
        emit_block(ast, iseq);
        break;
    case NDTYPE_TYPEDBLOCK:
        emit_typed_block(ast, iseq);
        break;
    case NDTYPE_RETURN:
        emit_return(ast, iseq);
        break;
    case NDTYPE_BREAK:
        emit_break(ast, iseq);
        break;
    case NDTYPE_VARIABLE:
        emit_load(ast, iseq, use_ret);
        break;
    case NDTYPE_FUNCCALL:
        emit_func_call(ast, iseq, use_ret);
        break;
    case NDTYPE_FUNCDEF:
        emit_func_def(ast, iseq);
        break;
    case NDTYPE_VARDECL:
        emit_vardecl(ast, iseq);
        break;
    case NDTYPE_NONENODE:
        break;
    default:
        error("??? in gen");
    }
}

static void emit_num(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeNumber *n = (NodeNumber *)ast;

    if(type_is(CAST_AST(n)->ctype, CTYPE_DOUBLE)) {
        int key = lpool_push_float(ltable, n->fnumber);

        push_fpush(iseq, key);
    }
    else {
        if(n->number == 0) {
            push_0arg(iseq, OP_PUSHCONST_0);
        }
        else if(n->number == 1) {
            push_0arg(iseq, OP_PUSHCONST_1);
        }
        else if(n->number == 2) {
            push_0arg(iseq, OP_PUSHCONST_2);
        }
        else if(n->number == 3) {
            push_0arg(iseq, OP_PUSHCONST_3);
        }
        else
            push_ipush(iseq, n->number);
    }

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

void emit_bool(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeBool *b = (NodeBool *)ast;

    if(b->boolean)
        push_0arg(iseq, OP_PUSHTRUE);
    else
        push_0arg(iseq, OP_PUSHFALSE);

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

void emit_char(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeChar *c = (NodeChar *)ast;

    // vcpush(OP_PUSH, (char)c->ch);
}

void emit_string(Ast *ast, Bytecode *iseq, bool use_ret) {
    int key = lpool_push_str(ltable, ((NodeString *)ast)->string);

    push_strset(iseq, key);

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

void emit_list(Ast *ast, Bytecode *iseq) {
    NodeList *l = (NodeList *)ast;

    for(int i = l->nsize - 1; i >= 0; i--)
        gen((Ast *)l->elem->data[i], iseq, true);

    push_list_set(iseq, l->nsize);
}

void emit_struct_init(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeStructInit *s = (NodeStructInit *)ast;

    push_structset(iseq, CAST_AST(s)->ctype->strct.nfield);

    // TODO
}

void emit_listaccess(Ast *ast, Bytecode *iseq) {
    NodeSubscript *l = (NodeSubscript *)ast;

    gen(l->index, iseq, true);
    gen(l->ls, iseq, true);

    push_0arg(iseq, OP_SUBSCR);
}

static void emit_tuple(Ast *ast, Bytecode *iseq) {
    NodeTuple *t = (NodeTuple *)ast;

    for(int i = (int)t->nsize - 1; i >= 0; i--)
        gen((Ast *)t->exprs->data[i], iseq, true);

    // vcpush(OP_TUPLESET, t->nsize);
}

static void emit_binop(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeBinop *b = (NodeBinop *)ast;

    if(b->impl != NULL) {
        gen((Ast *)b->impl, iseq, use_ret);
        goto fin;
    }

    gen(b->left, iseq, true);
    gen(b->right, iseq, true);

    if(type_is(b->left->ctype, CTYPE_INT)) {
        switch(b->op) {
        case BIN_ADD:
            push_0arg(iseq, OP_ADD);
            break;
        case BIN_SUB:
            push_0arg(iseq, OP_SUB);
            break;
        case BIN_MUL:
            push_0arg(iseq, OP_MUL);
            break;
        case BIN_DIV:
            push_0arg(iseq, OP_DIV);
            break;
        case BIN_MOD:
            push_0arg(iseq, OP_MOD);
            break;
        case BIN_EQ:
            push_0arg(iseq, OP_EQ);
            break;
        case BIN_NEQ:
            push_0arg(iseq, OP_NOTEQ);
            break;
        case BIN_LOR:
            push_0arg(iseq, OP_LOGOR);
            break;
        case BIN_LAND:
            push_0arg(iseq, OP_LOGAND);
            break;
        case BIN_LT:
            push_0arg(iseq, OP_LT);
            break;
        case BIN_LTE:
            push_0arg(iseq, OP_LTE);
            break;
        case BIN_GT:
            push_0arg(iseq, OP_GT);
            break;
        case BIN_GTE:
            push_0arg(iseq, OP_GTE);
            break;
        }
    }
    else if(type_is(b->left->ctype, CTYPE_DOUBLE)){
        switch(b->op) {
        case BIN_ADD:
            push_0arg(iseq, OP_FADD);
            break;
        case BIN_SUB:
            push_0arg(iseq, OP_FSUB);
            break;
        case BIN_MUL:
            push_0arg(iseq, OP_FMUL);
            break;
        case BIN_DIV:
            push_0arg(iseq, OP_FDIV);
            break;
        case BIN_MOD:
            push_0arg(iseq, OP_FMOD);
            break;
        case BIN_EQ:
            push_0arg(iseq, OP_FEQ);
            break;
        case BIN_NEQ:
            push_0arg(iseq, OP_FNOTEQ);
            break;
        case BIN_LOR:
            push_0arg(iseq, OP_FLOGOR);
            break;
        case BIN_LAND:
            push_0arg(iseq, OP_FLOGAND);
            break;
        case BIN_LT:
            push_0arg(iseq, OP_FLT);
            break;
        case BIN_LTE:
            push_0arg(iseq, OP_FLTE);
            break;
        case BIN_GT:
            push_0arg(iseq, OP_FGT);
            break;
        case BIN_GTE:
            push_0arg(iseq, OP_FGTE);
            break;
        }
    }
    else if(type_is(b->left->ctype, CTYPE_STRING)){
        switch(b->op) {
        case BIN_ADD:
            push_0arg(iseq, OP_STRCAT);
            break;
        default: break;
        }
    }

fin:
    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

void emit_member(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeMember *m = (NodeMember *)ast;

    gen(m->left, iseq, true);

    NodeVariable *rhs = (NodeVariable *)m->right;

    if(type_is(m->left->ctype, CTYPE_LIST)) {
        if(strcmp(rhs->name, "len") == 0) {
            return push_0arg(iseq, OP_LISTLENGTH);
        }
    }

    int i = 0;
    for(; i < m->left->ctype->strct.nfield; ++i) {
        if(strncmp(m->left->ctype->strct.field[i]->name,
                   rhs->name,
                   strlen(m->left->ctype->strct.field[i]->name)) == 0) {
            break;
        }
    }
    push_member_load(iseq, i);
}

static void emit_unary_neg(NodeUnaop *u, Bytecode *iseq) {
    if(type_is(((Ast *)u)->ctype, CTYPE_INT)) {
        push_0arg(iseq, OP_INEG);
    }
    else {  // float
        push_0arg(iseq, OP_FNEG);
    }
}

void emit_unaop(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeUnaop *u = (NodeUnaop *)ast;

    gen(u->expr, iseq, true);

    switch(u->op) {
    case UNA_INC:
        push_0arg(iseq, OP_INC);
        break;
    case UNA_DEC:
        push_0arg(iseq, OP_DEC);
        break;
    case UNA_MINUS:
        emit_unary_neg(u, iseq);
        break;
    default:
        mxc_unimplemented("sorry");
    }

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

static void emit_assign(Ast *ast, Bytecode *iseq) {
    // debug("called assign\n");
    NodeAssignment *a = (NodeAssignment *)ast;

    gen(a->src, iseq, true);

    if(a->dst->type == NDTYPE_SUBSCR)
        emit_listaccess_store(a->dst, iseq);
    else if(a->dst->type == NDTYPE_MEMBER)
        emit_member_store(a->dst, iseq);
    else
        emit_store(a->dst, iseq);
}

static void emit_store(Ast *ast, Bytecode *iseq) {
    NodeVariable *v = (NodeVariable *)ast;

    push_store(iseq, v->vid, v->isglobal);
}

static void emit_member_store(Ast *ast, Bytecode *iseq) {
    NodeMember *m = (NodeMember *)ast;

    gen(m->left, iseq, true);

    NodeVariable *rhs = (NodeVariable *)m->right;

    size_t i = 0;
    for(; i < m->left->ctype->strct.nfield; ++i) {
        if(strncmp(
               m->left->ctype->strct.field[i]->name,
               rhs->name,
               strlen(m->left->ctype->strct.field[i]->name)
           ) == 0) {
            break;
        }
    }

    push_member_store(iseq, i);
}

static void emit_listaccess_store(Ast *ast, Bytecode *iseq) {
    NodeSubscript *l = (NodeSubscript *)ast;

    gen(l->index, iseq, true);
    gen(l->ls, iseq, true);

    push_0arg(iseq, OP_SUBSCR_STORE);
}

static void emit_func_def(Ast *ast, Bytecode *iseq) {
    NodeFunction *f = (NodeFunction *)ast;

    Bytecode *fn_iseq = New_Bytecode();

    for(int n = f->finfo.args->vars->len - 1; n >= 0; n--) {
        NodeVariable *a = f->finfo.args->vars->data[n];
        emit_store((Ast *)a, fn_iseq);
    }

    if(f->block->type == NDTYPE_BLOCK) {
        NodeBlock *b = (NodeBlock *)f->block;
        for(size_t i = 0; i < b->cont->len; i++) {
            gen(b->cont->data[i],
                fn_iseq,
                false);
        }

        push_0arg(fn_iseq, OP_PUSHNULL);
    }
    else {
        gen(f->block, fn_iseq, true);
    }

    push_0arg(fn_iseq, OP_RET);

    userfunction *fn_object = New_Userfunction(fn_iseq, f->lvars);

    int key = lpool_push_userfunc(ltable, fn_object);

    push_functionset(iseq, key);

    emit_store((Ast *)f->fnvar, iseq);
}

static void emit_if(Ast *ast, Bytecode *iseq) {
    NodeIf *i = (NodeIf *)ast;

    gen(i->cond, iseq, true);

    size_t cpos = iseq->len;
    push_jmpneq(iseq, 0);

    gen(i->then_s, iseq, i->isexpr);

    if(i->else_s) {
        size_t then_epos = iseq->len;
        push_jmp(iseq, 0); // goto if statement end

        size_t else_spos = iseq->len;
        replace_int32(cpos, iseq, else_spos);

        gen(i->else_s, iseq, i->isexpr);

        size_t epos = iseq->len;
        replace_int32(then_epos, iseq, epos);
    }
    else {
        size_t pos = iseq->len;
        replace_int32(cpos, iseq, pos);
    }
}

void emit_for(Ast *ast, Bytecode *iseq) {
    /*
     *  for i in [10, 20, 30, 40] {}
     */
    NodeFor *f = (NodeFor *)ast;

    gen(f->iter, iseq, true);

    size_t loop_begin = iseq->len;

    size_t pos = iseq->len;
    push_iter_next(iseq, 0);

    for(int i = 0; i < f->vars->len; i++) {
        emit_store(f->vars->data[0], iseq); 
    }

    gen(f->body, iseq, false);

    push_jmp(iseq, loop_begin);

    size_t loop_end = iseq->len;
    replace_int32(pos, iseq, loop_end);
}

void emit_while(Ast *ast, Bytecode *iseq) {
    NodeWhile *w = (NodeWhile *)ast;

    size_t begin = iseq->len;

    gen(w->cond, iseq, true);

    size_t pos = iseq->len;
    push_jmpneq(iseq, 0);

    gen(w->body, iseq, false);

    push_jmp(iseq, begin);

    size_t end = iseq->len;
    replace_int32(pos, iseq, end);

    if(loop_stack->len != 0) {
        int breakp = (intptr_t)vec_pop(loop_stack);
        replace_int32(breakp, iseq, end);
    }
}

static void emit_return(Ast *ast, Bytecode *iseq) {
    gen(((NodeReturn *)ast)->cont, iseq, true);

    push_0arg(iseq, OP_RET);
}

static void emit_break(Ast *ast, Bytecode *iseq) {
    vec_push(loop_stack, (void *)(intptr_t)iseq->len);

    push_jmp(iseq, 0);
}

static void emit_func_call(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeFnCall *f = (NodeFnCall *)ast;

    if(((NodeVariable *)f->func)->finfo.isbuiltin) {
        return emit_bltinfunc_call(f, iseq, use_ret);
    }

    for(int i = 0; i < f->args->len; ++i)
        gen((Ast *)f->args->data[i], iseq, true);

    gen(f->func, iseq, true);

    push_0arg(iseq, OP_CALL);

    if(f->failure_block) {
        int erpos = iseq->len;
        push_jmp_nerr(iseq, 0);

        gen(f->failure_block, iseq, true);

        int epos = iseq->len;
        replace_int32(erpos, iseq, epos);
    }

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

static void emit_bltinfunc_call(NodeFnCall *f, Bytecode *iseq, bool use_ret) {
    NodeVariable *fn = (NodeVariable *)f->func;

    if(fn->finfo.fnkind == BLTINFN_PRINT) {
        return emit_bltinfncall_print(f, iseq, use_ret);
    }

    if(fn->finfo.fnkind == BLTINFN_PRINTLN) {
        return emit_bltinfncall_println(f, iseq, use_ret);
    }

    for(int i = 0; i < f->args->len; ++i)
        gen((Ast *)f->args->data[i], iseq, true);

    enum BLTINFN callfn = fn->finfo.fnkind;

    push_bltinfn_set(iseq, callfn);

    push_bltinfn_call(iseq, f->args->len);

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

static void emit_bltinfncall_println(
        NodeFnCall *f,
        Bytecode *iseq,
        bool use_ret
    ) {
    NodeVariable *fn = (NodeVariable *)f->func;

    enum BLTINFN callfn = fn->finfo.fnkind;

    for(int i = f->args->len - 1; i >= 0; --i) {
        gen((Ast *)f->args->data[i], iseq, true);

        int ret = show_from_type(((Ast *)f->args->data[i])->ctype->type);

        if(ret == -1) {
            // TODO
        } 
        else if(ret != 0) {
            push_0arg(iseq, ret);
        }
    }

    push_bltinfn_set(iseq, callfn);

    push_bltinfn_call(iseq, f->args->len);

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

static void emit_bltinfncall_print(
        NodeFnCall *f,
        Bytecode *iseq,
        bool use_ret
    ) {
    NodeVariable *fn = (NodeVariable *)f->func;

    enum BLTINFN callfn = fn->finfo.fnkind;

    for(int i = f->args->len - 1; i >= 0; --i) {
        gen((Ast *)f->args->data[i], iseq, true);

        int ret = show_from_type(((Type *)fn->finfo.ftype->fnarg->data[i])->type);

        if(ret == -1) {
            // TODO
        } 
        else if(ret != 0) {
            push_0arg(iseq, ret);
        }
    }

    push_bltinfn_set(iseq, callfn);

    push_bltinfn_call(iseq, f->args->len);

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

static void emit_block(Ast *ast, Bytecode *iseq) {
    NodeBlock *b = (NodeBlock *)ast;

    for(int i = 0; i < b->cont->len; ++i)
        gen((Ast *)b->cont->data[i], iseq, false);
}

static void emit_typed_block(Ast *ast, Bytecode *iseq) {
    NodeBlock *b = (NodeBlock *)ast;

    for(int i = 0; i < b->cont->len; ++i) {
        gen((Ast *)b->cont->data[i],
             iseq,
             i == b->cont->len - 1 ? true: false);
    }
}

static void emit_vardecl_block(NodeVardecl *v, Bytecode *iseq) {
    for(int i = 0; i < v->block->len; ++i) {
        emit_vardecl(v->block->data[i], iseq);
    }
}

static void emit_vardecl(Ast *ast, Bytecode *iseq) {
    NodeVardecl *v = (NodeVardecl *)ast;

    if(v->is_block) {
        emit_vardecl_block(v, iseq);
        return;
    }

    if(v->init != NULL) {
        gen(v->init, iseq, true);

        emit_store((Ast *)v->var, iseq);
    }
}

static void emit_load(Ast *ast, Bytecode *iseq, bool use_ret) {
    NodeVariable *v = (NodeVariable *)ast;

    push_load(iseq, v->vid, v->isglobal);

    if(!use_ret)
        push_0arg(iseq, OP_POP);
}

static int show_from_type(enum CTYPE ty) {
    switch(ty) {
    case CTYPE_INT:     return OP_SHOWINT;
    case CTYPE_DOUBLE:  return OP_SHOWFLOAT;
    case CTYPE_BOOL:    return OP_SHOWBOOL;
    default:            return 0;
    }

    return -1;
}
