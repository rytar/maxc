#include "parser.h"
#include "error/error.h"
#include "lexer.h"

static Vector *eval(void);
static Ast *statement(void);
static Ast *expr(void);
static Ast *func_def(void);
static Ast *make_block(void);
static Ast *make_if(bool);
static Ast *make_for(void);
static Ast *make_while(void);
static Ast *make_return(void);
static Ast *make_break(void);
static Ast *make_object(void);
static Ast *make_import(void);
static void make_typedef(void);

static Ast *expr_assign(void);
static Ast *expr_equality(void);
static Ast *expr_logic_or(void);
static Ast *expr_logic_and(void);
static Ast *expr_comp(void);
static Ast *expr_bitshift(void);
static Ast *expr_add(void);
static Ast *expr_mul(void);
static Ast *expr_unary(void);
static Ast *expr_unary_postfix(void);
static Ast *expr_primary(void);

static Ast *new_object(void);
static Ast *var_decl(bool);
static Ast *expr_do(void);
static Ast *expr_char();
static Ast *expr_num(Token *);
static Ast *expr_unary(void);
static Type *eval_type(void);

static Vector *tokens = NULL;
static Vector *tokens_stack;
static Vector *pos_stack;
static int pos = 0;
static int nenter = 0;

#define Step() (++pos)
#define Cur_Token() ((Token *)tokens->data[pos])
#define Get_Step_Token() ((Token *)tokens->data[pos++])
#define Cur_Token_Is(tk) ((Cur_Token()->kind) == (tk))

#define Get_Cur_Line() (Cur_Token()->start.line)

static Vector *enter(Vector *tk) {
    vec_push(tokens_stack, tokens);
    vec_push(pos_stack, (void *)(intptr_t)pos);

    tokens = tk;
    pos = 0;

    Vector *result = eval();

    tokens = vec_pop(tokens_stack);
    pos = (intptr_t)vec_pop(pos_stack);

    nenter++;

    Delete_Vector(tk);

    return result;
}

Vector *parser_run(Vector *_token) {
    tokens_stack = New_Vector();
    pos_stack = New_Vector();

    return enter(_token);
}

static bool skip(enum TKIND tk) {
    if(Cur_Token()->kind == tk) {
        ++pos;
        return true;
    }

    return false;
}

static bool skip2(enum TKIND tk1, enum TKIND tk2) {
    int tmp = pos;

    if(Cur_Token()->kind == tk1) {
        ++pos;
        if(Cur_Token()->kind == tk2) {
            ++pos;
            return true;
        }
    }

    pos = tmp;
    return false;
}

static Token *see(int);

static Token *expect(enum TKIND tk) {
    Token *cur = Cur_Token();

    if(cur->kind == tk) {
        ++pos;
        return cur;
    }
    else {
        expect_token(see(-1)->start, see(-1)->end, tk2str(tk));
        return NULL;
    }
}

static Token *expect_type(enum TKIND tk) {
    Token *cur = Cur_Token();

    if(cur->kind == tk) {
        ++pos;
        return cur;
    }
    else {
        expect_token(see(0)->start, see(0)->end, tk2str(tk));
        return NULL;
    }
}

static Token *see(int p) { return tokens->data[pos + p]; }

static Vector *eval() {
    Vector *program = New_Vector();

    while(!Cur_Token_Is(TKIND_End)) {
        Ast *st = statement();

        if(st) {
            vec_push(program, st);
        }

        if(Ast_isexpr(st)) {
            expect(TKIND_Semicolon);
        }
    }

    return program;
}

static Ast *statement() {
    if(Cur_Token_Is(TKIND_Lbrace))
        return make_block();
    else if(skip(TKIND_For))
        return make_for();
    else if(skip(TKIND_While))
        return make_while();
    else if(skip(TKIND_If))
        return make_if(false);
    else if(skip(TKIND_Return))
        return make_return();
    else if(skip(TKIND_Break))
        return make_break();
    else if(skip(TKIND_Let))
        return var_decl(false);
    else if(skip(TKIND_Const))
        return var_decl(true);
    else if(skip(TKIND_Fn))
        return func_def();
    else if(skip(TKIND_Object))
        return make_object();
    else if(skip(TKIND_Import))
        return make_import();
    else if(skip(TKIND_Typedef)) {
        make_typedef();
        return NULL;
    }
    else
        return expr();
}

static Ast *expr() { return expr_assign(); }

static Ast *func_def() {
    bool is_operator = false;
    bool is_generic = false;
    enum TKIND op = -1;

    Vector *typevars = NULL;

    /*
     *  fn <T> main(): T
     *     ^^^
     */
    if(skip(TKIND_Lt)) {    // <
        is_generic = true;

        typevars = New_Vector();

        for(int i = 0; !skip(TKIND_Gt); i++) {    // >
            if(i > 0) {
                expect(TKIND_Comma);
            }
            char *name = Cur_Token()->value;
            Step();
            vec_push(typevars, New_Type_Variable(name));
        }
    }

    if(Cur_Token()->kind == TKIND_BQLIT) {
        is_operator = true;
        op = Cur_Token()->cont;

        if(op == -1) {
            error("operators that cannot be overloaded");
        }
    }

    char *name = Cur_Token()->value;
    Step();

    // fn main(): typename {
    //        ^
    if(!expect(TKIND_Lparen)) {
        return NULL;
    }

    Varlist *args = New_Varlist();
    var_t arg_info;
    func_t fn_arg_info;
    Vector *argtys = New_Vector();

    if(!skip(TKIND_Rparen))
        /*
         * fn main(a: int, b: int): int {
         *         ^^^^^^^^^^^^^^
         *
         * fn main(a, b: int): int {
         *         ^^^^^^^^^
         */
        for(;;) {
            Vector *argnames = New_Vector();

            char *arg_name = Get_Step_Token()->value;
            vec_push(argnames, arg_name);

            if(skip(TKIND_Comma)) {
                for(;;) {
                    char *name = Get_Step_Token()->value;
                    vec_push(argnames, name);

                    if(Cur_Token_Is(TKIND_Colon)) {
                        break;
                    }

                    expect(TKIND_Comma);
                }
            }

            expect(TKIND_Colon);

            Type *arg_ty = eval_type();

            for(int i = 0; i < argnames->len; ++i)
                vec_push(argtys, arg_ty);

            if(type_is(arg_ty, CTYPE_FUNCTION))
                fn_arg_info = New_Func_t(arg_ty, is_generic);
            else
                arg_info = (var_t){arg_ty};

            Varlist *a = New_Varlist();

            for(int i = 0; i < argnames->len; ++i) {
                varlist_push(
                    a,
                    type_is(arg_ty, CTYPE_FUNCTION)
                        ? new_node_variable_with_func(
                              argnames->data[i],
                              fn_arg_info
                          )
                        : new_node_variable_with_var(
                              argnames->data[i],
                              arg_info
                          )
                );
            }

            varlist_mulpush(args, a);

            if(skip(TKIND_Rparen))
                break;
            expect(TKIND_Comma);
        }

    // fn main(): int {
    //          ^^^^^
    Type *ret_ty = skip(TKIND_Colon) ? eval_type() : NULL;

    Type *fntype = New_Type_Function(argtys, ret_ty);

    Ast *block = NULL;

    if(Cur_Token_Is(TKIND_Lbrace)) {        // {
        block = make_block();

        if(ret_ty == NULL)
            fntype->fnret = mxcty_none;
    }
    else if(Cur_Token_Is(TKIND_Assign)){    // =
        Step();

        block = expr();

        expect(TKIND_Semicolon);

        if(ret_ty == NULL)
            fntype->fnret = New_Type(CTYPE_UNINFERRED);
    }
    else {
        unexpected_token(see(0)->start,
                         see(0)->end,
                         Cur_Token()->value,
                         "=", "{", NULL);
    }

    func_t finfo = New_Func_t_With_Varlist(args, fntype, is_generic);

    NodeVariable *function = new_node_variable_with_func(name, finfo);

    NodeFunction *node = new_node_function(function, finfo, block, typevars);

    if(is_operator) {
        node->op = op;
    }

    return (Ast *)node;
}

static Ast *var_decl_block(bool isconst) {
    Vector *block = New_Vector();

    for(;;) {
        Type *ty = NULL;
        Ast *init = NULL;
        var_t info;
        func_t finfo;
        NodeVariable *var = NULL;

        char *name = Cur_Token()->value;
        Step();

        if(skip(TKIND_Colon)) {
            ty = eval_type();
        }
        else {
            ty = New_Type(CTYPE_UNINFERRED);
        }

        int vattr = 0;

        if(isconst) {
            vattr |= VARATTR_CONST;
        }

        if(skip(TKIND_Assign)) {
            init = expr();
        }
        else if(isconst) {
            error_at(see(0)->start, see(0)->end, "const must initialize");

            init = NULL;
        }
        else {
            init = NULL;
        }

        if(ty != NULL) {
            if(type_is(ty, CTYPE_FUNCTION))
                finfo = New_Func_t(ty, false);
            else
                info = (var_t){ty};

            var = type_is(ty, CTYPE_FUNCTION)
                ? new_node_variable_with_func(name, finfo)
                : new_node_variable_with_var(name, info);
        }
        else {
            var = NULL;
        }

        expect(TKIND_Semicolon);

        vec_push(block, new_node_vardecl(var, init, NULL));

        if(skip(TKIND_Rbrace)) break;
    }

    return (Ast *)new_node_vardecl(NULL, NULL, block);
}

static Ast *var_decl(bool isconst) {
    if(skip(TKIND_Lbrace)) {
        return var_decl_block(isconst);
    }

    var_t info;
    func_t finfo;

    Ast *init = NULL;

    Type *ty = NULL;
    NodeVariable *var = NULL;

    char *name = Get_Step_Token()->value;

    /*
     *  let a(: int) = 100;
     *        ^^^^^
     */
    if(skip(TKIND_Colon)) {
        ty = eval_type();
    }
    else
        ty = New_Type(CTYPE_UNINFERRED);

    int vattr = 0;

    if(isconst)
        vattr |= VARATTR_CONST;

    /*
     *  let a: int = 100;
     *             ^
     */
    if(skip(TKIND_Assign)) {
        init = expr();
    }
    else if(isconst) {
        error_at(see(0)->start, see(0)->end, "const must initialize");

        init = NULL;
    }
    else {
        init = NULL;
    }

    if(ty != NULL) {
        if(type_is(ty, CTYPE_FUNCTION))
            finfo = New_Func_t(ty, false);
        else
            info = (var_t){ty};

        var = type_is(ty, CTYPE_FUNCTION)
                  ? new_node_variable_with_func(name, finfo)
                  : new_node_variable_with_var(name, info);
    }
    else {
        var = NULL;
    }

    expect(TKIND_Semicolon);

    return (Ast *)new_node_vardecl(var, init, NULL);
}

static Ast *make_object() {
    /*
     *  object TagName {
     *      a: int,
     *      b: string
     *  }
     *
     */
    char *tag = Get_Step_Token()->value;

    expect(TKIND_Lbrace);

    Vector *decls = New_Vector();

    if(skip(TKIND_Rbrace))
        return (Ast *)new_node_object(tag, decls);

    for(;;) {
        char *name = Get_Step_Token()->value;
        expect(TKIND_Colon);

        Type *ty = eval_type();

        vec_push(decls, new_node_variable_with_var(name, (var_t){ty}));

        if(skip(TKIND_Rbrace))
            break;

        expect(TKIND_Comma);
    }

    return (Ast *)new_node_object(tag, decls);
}

static void make_ast_from_mod(Vector *s, char *name) {
    char path[512];

    sprintf(path, "./lib/%s.mxc", name);

    char *src = read_file(path);
    if(!src) {
        memset(path, 0, 512);
        sprintf(path, "./%s.mxc", name);

        src = read_file(path);
        if(!src) {
            error("lib %s: not found", name);
            return;
        }
    }

    Vector *token = lexer_run(src, name);

    Vector *AST = enter(token);

    for(int i = 0; i < AST->len; i++) {
        vec_push(s, AST->data[i]);
    }
}

static Ast *make_import() {
    Vector *mod_names = New_Vector();
    Vector *statements = New_Vector();

    char *mod = Get_Step_Token()->value;

    expect(TKIND_Semicolon);

    vec_push(mod_names, mod);

    for(int i = 0; i < mod_names->len; i++) {
        make_ast_from_mod(statements, (char *)mod_names->data[i]);
    }

    return (Ast *)new_node_block_nonscope(statements);
}

static Type *eval_type() {
    Type *ty;

    if(skip(TKIND_Lparen)) { // tuple
        ty = New_Type(CTYPE_TUPLE);

        for(;;) {
            vec_push(ty->tuple, eval_type());

            if(skip(TKIND_Rparen))
                break;

            expect(TKIND_Comma);
        }
    }
    else if(skip(TKIND_TInt))
        ty = mxcty_int;
    else if(skip(TKIND_TUint))
        ty = New_Type(CTYPE_UINT);
    else if(skip(TKIND_TBool))
        ty = mxcty_bool;
    else if(skip(TKIND_TString))
        ty = mxcty_string;
    else if(skip(TKIND_TChar))
        ty = mxcty_char;
    else if(skip(TKIND_TFloat))
        ty = mxcty_float;
    else if(skip(TKIND_TNone)) // TODO :only function rettype
        ty = mxcty_none;
    else if(skip(TKIND_Fn)) {
        expect(TKIND_Lparen);

        Vector *arg = New_Vector();

        while(!skip(TKIND_Rparen)) {
            vec_push(arg, eval_type());
            if(skip(TKIND_Rparen))
                break;
            expect(TKIND_Comma);
        }

        expect(TKIND_Colon);
        Type *ret = eval_type();

        ty = New_Type_Function(arg, ret);
    }
    else {
        char *tk = Cur_Token()->value;

        Step();

        ty = New_Type_Unsolved(tk);
    }

    for(;;) {
        if(skip2(TKIND_Lboxbracket, TKIND_Rboxbracket))
            ty = New_Type_With_Ptr(ty);
        else
            break;
    }

    /*
     *  int?
     *     ^
     */
    if(skip(TKIND_Question)) {
        ty = (Type *)New_MxcOptional(ty);
    }

    return ty;
}

Ast *make_assign(Ast *dst, Ast *src) {
    if(!dst)
        return NULL;

    return (Ast *)new_node_assign(dst, src);
}

Ast *make_assigneq(char *op, Ast *dst, Ast *src) {
    (void)op;
    (void)dst;
    (void)src;
    return NULL; // TODO
}

static Ast *make_block() {
    expect(TKIND_Lbrace);
    Vector *cont = New_Vector();

    Ast *b;

    for(;;) {
        if(skip(TKIND_Rbrace))
            break;
        b = statement();

        if(Ast_isexpr(b)) {
            expect(TKIND_Semicolon);
        }

        vec_push(cont, b);
    }

    return (Ast *)new_node_block(cont);
}

static Ast *make_typed_block() {
    expect(TKIND_Lbrace);
    Vector *cont = New_Vector();

    Ast *b;

    for(;;) {
        if(skip(TKIND_Rbrace))
            break;

        b = statement();

        if(Ast_isexpr(b)) {
            expect(TKIND_Semicolon);
        }

        vec_push(cont, b);
    }

    return (Ast *)new_node_typedblock(cont);
}

static Ast *make_if(bool isexpr) {
    Ast *cond = expr();

    Ast *then = isexpr ? expr() : make_block();

    if(skip(TKIND_Else)) {
        Ast *el;

        if(skip(TKIND_If))
            el = make_if(isexpr);
        else
            el = isexpr ? expr() : make_block();

        return (Ast *)new_node_if(cond, then, el, isexpr);
    }

    return (Ast *)new_node_if(cond, then, NULL, isexpr);
}

static Ast *make_for() {
    /*
     *  for i in iter() { }
     */

    Vector *v = New_Vector();

    do {
        if(Cur_Token()->kind == TKIND_Identifer) {
            Type *ty = New_Type(CTYPE_UNINFERRED); 
            vec_push(v, new_node_variable_with_var(Cur_Token()->value, (var_t){ty}));
        }
        else {
            error_at(see(0)->start, see(0)->end, "expected identifer");
        }

        Step();
    } while(!skip(TKIND_In));

    Ast *iter = expr();

    Ast *body = statement();

    return (Ast *)new_node_for(v, iter, body);
}

static Ast *make_while() {
    Ast *cond = expr();

    Ast *body = make_block();

    return (Ast *)new_node_while(cond, body);
}

static Ast *make_return() {
    Ast *e = expr();
    NodeReturn *ret = new_node_return(e);
    if(e->type != NDTYPE_NONENODE)
        expect(TKIND_Semicolon);

    return (Ast *)ret;
}


static Ast *make_break() {
    NodeBreak *ret = new_node_break();

    expect(TKIND_Semicolon);

    return (Ast *)ret;
}

static void make_typedef() {
    mxc_unimplemented("typedef");
    /*
    std::string to = token.get().value;
    Step();
    expect(TKIND_Assign);
    Type *from = eval_type();
    expect(TKIND_Semicolon);
    typemap[to] = from; */
}

static Ast *expr_char() {
    Token *cur = Get_Step_Token();
    return (Ast *)new_node_char(cur->cont);
}

static Ast *expr_num(Token *tk) {
    if(strchr(tk->value, '.'))
        return (Ast *)new_node_number_float(atof(tk->value));
    else
        return (Ast *)new_node_number_int(atoll(tk->value));
}

static Ast *expr_string(Token *tk) { return (Ast *)new_node_string(tk->value); }

static Ast *expr_var(Token *tk) {
    if(tk->kind != TKIND_Identifer) {
        unexpected_token(tk->start,
                         tk->end,
                         tk->value,
                         "Identifer", NULL);
    }

    return (Ast *)new_node_variable(tk->value, 0);
}

static Ast *expr_assign() {
    Ast *left = expr_logic_or();

    if(Cur_Token_Is(TKIND_Assign)) {
        if(left == NULL) {
            return NULL;
        }

        Step();
        left = make_assign(left, expr_assign());
    }

    return left;
}

static Ast *expr_logic_or() {
    Ast *left = expr_logic_and();
    Ast *t;

    for(;;) {
        if(Cur_Token_Is(TKIND_LogOr)) {
            Step();
            t = expr_logic_and();
            left = (Ast *)new_node_binary(BIN_LOR, left, t);
        }
        else if(Cur_Token_Is(TKIND_KOr)) {
            Step();
            t = expr_logic_and();
            left = (Ast *)new_node_binary(BIN_LOR, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_logic_and() {
    Ast *left = expr_equality();
    Ast *t;

    for(;;) {
        if(Cur_Token_Is(TKIND_LogAnd)) {
            Step();
            t = expr_equality();
            left = (Ast *)new_node_binary(BIN_LAND, left, t);
        }
        else if(Cur_Token_Is(TKIND_KAnd)) {
            Step();
            t = expr_equality();
            left = (Ast *)new_node_binary(BIN_LAND, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_equality() {
    Ast *left = expr_comp();

    for(;;) {
        if(Cur_Token_Is(TKIND_Eq)) {
            Step();
            Ast *t = expr_comp();
            left = (Ast *)new_node_binary(BIN_EQ, left, t);
        }
        else if(Cur_Token_Is(TKIND_Neq)) {
            Step();
            Ast *t = expr_comp();
            left = (Ast *)new_node_binary(BIN_NEQ, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_comp() {
    Ast *left = expr_bitshift();
    Ast *t;

    for(;;) {
        if(Cur_Token_Is(TKIND_Lt)) {
            Step();
            t = expr_bitshift();
            left = (Ast *)new_node_binary(BIN_LT, left, t);
        }
        else if(Cur_Token_Is(TKIND_Gt)) {
            Step();
            t = expr_bitshift();
            left = (Ast *)new_node_binary(BIN_GT, left, t);
        }
        else if(Cur_Token_Is(TKIND_Lte)) {
            Step();
            t = expr_bitshift();
            left = (Ast *)new_node_binary(BIN_LTE, left, t);
        }
        else if(Cur_Token_Is(TKIND_Gte)) {
            Step();
            t = expr_bitshift();
            left = (Ast *)new_node_binary(BIN_GTE, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_bitshift() {
    Ast *left = expr_add();
    Ast *t;

    for(;;) {
        if(Cur_Token_Is(TKIND_Lshift)) {
            Step();
            t = expr_add();
            left = (Ast *)new_node_binary(BIN_LSHIFT, left, t);
        }
        else if(Cur_Token_Is(TKIND_Rshift)) {
            Step();
            t = expr_add();
            left = (Ast *)new_node_binary(BIN_RSHIFT, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_add() {
    Ast *left = expr_mul();

    for(;;) {
        if(Cur_Token_Is(TKIND_Plus)) {
            Step();
            Ast *t = expr_mul();
            left = (Ast *)new_node_binary(BIN_ADD, left, t);
        }
        else if(Cur_Token_Is(TKIND_Minus)) {
            Step();
            Ast *t = expr_mul();
            left = (Ast *)new_node_binary(BIN_SUB, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_mul() {
    Ast *left = expr_unary();
    Ast *t;

    for(;;) {
        if(Cur_Token_Is(TKIND_Asterisk)) {
            Step();
            t = expr_unary();
            left = (Ast *)new_node_binary(BIN_MUL, left, t);
        }
        else if(Cur_Token_Is(TKIND_Div)) {
            Step();
            t = expr_unary();
            left = (Ast *)new_node_binary(BIN_DIV, left, t);
        }
        else if(Cur_Token_Is(TKIND_Mod)) {
            Step();
            t = expr_unary();
            left = (Ast *)new_node_binary(BIN_MOD, left, t);
        }
        else
            return left;
    }
}

static Ast *expr_unary() {
    enum TKIND tk = Cur_Token()->kind;
    enum UNAOP op = -1;

    switch(tk) {
    case TKIND_Inc:
        op = UNA_INC;
        break;
    case TKIND_Dec:
        op = UNA_DEC;
        break;
    case TKIND_Minus:
        op = UNA_MINUS;
        break;
    case TKIND_Bang:
        op = UNA_NOT;
        break;
    default:
        goto end;
    }

    Step();
    Ast *operand = expr_unary();

    return (Ast *)new_node_unary(op, operand);

end:
    return expr_unary_postfix();
}

static Ast *expr_unary_postfix() {
    Ast *left = expr_primary();

    for(;;) {
        if(Cur_Token_Is(TKIND_Dot)) {
            Step();

            Ast *memb = expr_var(Cur_Token());

            Step();
            /*
             *  a.function();
             *            ^
             */
            if(skip(TKIND_Lparen)) {
                Vector *args = New_Vector();
                vec_push(args, left);

                for(int i = 0; !skip(TKIND_Rparen); ++i) {
                    if(i > 0) {
                        expect(TKIND_Comma);
                    }

                    vec_push(args, expr());
                }

                left = (Ast *)new_node_fncall(memb, args, NULL);
            }
            else { // struct
                left = (Ast *)new_node_dotexpr(left, memb);
            }
        }
        else if(Cur_Token_Is(TKIND_Lboxbracket)) {
            Step();
            Ast *index = expr();

            expect(TKIND_Rboxbracket);

            left = (Ast *)new_node_subscript(left, index);
        }
        else if(Cur_Token_Is(TKIND_Lparen)) {
            Step();
            Vector *args = New_Vector();

            if(skip(TKIND_Rparen))
                ;
            else {
                for(;;) {
                    vec_push(args, expr());
                    if(skip(TKIND_Rparen))
                        break;
                    expect(TKIND_Comma);
                }
            }

            Ast *fail = NULL;

            if(skip2(TKIND_Dot, TKIND_FAILURE)) {
                fail = make_typed_block();
            }

            left = (Ast *)new_node_fncall(left, args, fail);
        }
        else
            return left;
    }
}

static Ast *expr_primary() {
    if(skip(TKIND_True)) {
        return (Ast *)new_node_bool(true);
    }
    else if(skip(TKIND_False)) {
        return (Ast *)new_node_bool(false);
    }
    else if(skip(TKIND_New))
        return new_object();
    else if(skip(TKIND_If))
        return make_if(true);
    else if(Cur_Token_Is(TKIND_Identifer)) {
        Ast *v = expr_var(Get_Step_Token());
        return v;
    }
    else if(Cur_Token_Is(TKIND_Num))
        return expr_num(Get_Step_Token());
    else if(Cur_Token_Is(TKIND_String))
        return expr_string(Get_Step_Token());
    else if(Cur_Token_Is(TKIND_Char))
        return expr_char();
    else if(Cur_Token_Is(TKIND_Lparen)) {
        Step();

        if(skip(TKIND_Rparen)) {
            return NULL;
        }

        Ast *left = expr();

        if(skip(TKIND_Comma)) { // tuple
            if(skip(TKIND_Rparen)) {
                error("error"); // TODO
                return NULL;
            }
            Vector *exs = New_Vector();
            Ast *a;
            Type *ty = New_Type(CTYPE_TUPLE);
            vec_push(exs, left);

            vec_push(ty->tuple, left->ctype);

            for(;;) {
                a = expr();
                vec_push(ty->tuple, a->ctype);
                vec_push(exs, a);
                if(skip(TKIND_Rparen))
                    return (Ast *)new_node_tuple(exs, exs->len, ty);
                expect(TKIND_Comma);
            }
        }

        if(!expect(TKIND_Rparen))
            Step();

        return left;
    }
    else if(Cur_Token_Is(TKIND_Lboxbracket)) {
        Step();
        if(Cur_Token_Is(TKIND_Rboxbracket)) { // TODO: Really?
            error("error");
            return NULL;
        }

        Vector *elem = New_Vector();
        Ast *a = expr();
        vec_push(elem, a);

        for(;;) {
            if(skip(TKIND_Rboxbracket))
                break;
            expect(TKIND_Comma);
            a = expr();
            vec_push(elem, a);
        }

        return (Ast *)new_node_list(elem, elem->len);
    }
    else if(Cur_Token_Is(TKIND_Semicolon)) {
        Step();
        return (Ast *)new_none_node();
    }
    else if(Cur_Token_Is(TKIND_End)) {
        exit(1);
    }

    error_at(see(0)->start, see(0)->end, "syntax error\n");
    Step();

    return NULL;
}

static Ast *new_object() {
    /*
     *  let a = new Data {
     *      member1: 100,
     *      member2: "hogehoge"
     *  };
     */
    char *tagname = Get_Step_Token()->value;

    Type *tag = New_Type_Unsolved(tagname);

    expect(TKIND_Lbrace);

    Vector *fields = New_Vector();
    Vector *inits = New_Vector();

    if(skip(TKIND_Rbrace))
        ;
    else
        for(;;) {
            vec_push(fields, expr_var(Get_Step_Token()));

            expect(TKIND_Colon);

            vec_push(inits, expr());

            if(skip(TKIND_Rbrace))
                break;
            expect(TKIND_Comma);
        }

    return (Ast *)new_node_struct_init(tag, fields, inits);
}
