#include"maxc.h"

Ast_v Parser::run(Token _token) {
    token = _token;
    //Ast *ast = statement();
    Ast_v program = eval();

    return program;
}

Ast_v Parser::eval() {
    Ast_v program;

    while(!token.is_type(TOKEN_TYPE_END)) {
        program.push_back(statement());

        token.skip(";");
    }
    return program;
}

Ast *Parser::statement() {
    if(token.is_value(";")) {
        token.step();
        return statement();
    }
    else if(token.skip("{"))
        return make_block();
    else if(token.is_value("if"))
        return make_if();
    else if(token.is_value("for"))
        return make_for();
    else if(token.is_value("while")) {
        return make_while();
    }
    else if(token.is_value("return")) {
        token.step();
        return make_return();
    }
    else if(is_func_def())
        return func_def();
    else if(is_var_decl())
        return var_decl();
    else
        return expr();
}

Ast *Parser::expr() {
    if(token.is_type(TOKEN_TYPE_IDENTIFER)) {
        if(token.see(1).value == "=")
            return assignment();
        else
            return expr_first();
    }
    return expr_first();
}

Ast *Parser::expr_first() {
    return expr_logic_or();
}

Ast *Parser::func_def() {
    var_type ty = eval_type();
    std::string name = token.get().value;
    token.step();

    if(token.skip("(")) {
        std::vector<arg_t> args;

        while(!token.skip(")")) {
            var_type arg_ty = eval_type();
            std::string arg_name = token.get().value;
            args.push_back((arg_t){arg_ty, arg_name});
            token.step();
            token.skip(",");
        }
        if(!token.skip("{"))
            error("??????????");
        Ast_v b;
        while(!token.skip("}")) {
            b.push_back(statement());
            token.skip(";");
        }

        return new Node_func_def(ty, name, args, b);
    }
    else {
        error("error");
        return nullptr;
    }
}

Ast *Parser::func_call() {
    std::string name = token.get().value;
    token.step();
    if(token.skip("(")) {
        Ast_v args;

        while(!token.skip(")")) {
            args.push_back(expr_first());
            //token.step();
            token.skip(",");
        }

        return new Node_func_call(name, args);
    }
    else {
        error("error");
        return nullptr;
    }
}

Ast *Parser::var_decl() {
    std::vector<var_t> decls;
    var_type ty = eval_type();
    Ast *init = nullptr;

    while(1) {
        std::string name = token.get().value;
        token.step();

        if(token.skip("="))
            init = expr_first();
        decls.push_back((var_t){ty, name, init});

        if(token.skip(";")) break;
        token.abs_skip(",");
    }

    return new Node_var_decl(decls);
}

var_type Parser::eval_type() {
    if(token.is_value("int")) {
        token.step();
        return TYPE_INT;
    }
    else if(token.is_value("void")) {
        token.step();
        return TYPE_VOID;
    }
    else {
        error("eval_type ?????");
        return (var_type)-1;
    }
}

Ast *Parser::assignment() {
    if(!token.is_type(TOKEN_TYPE_IDENTIFER))
        error("left is not identifer");
    Ast *dst = expr_var(token.get_step());
    Ast *src;
    //token.step();
    if(token.skip("="))
        src = expr();
    else
        error("????? in assignment");

    return new Node_assignment(dst, src);
}

Ast *Parser::make_block() {
    Ast_v cont;
    while(!token.skip("}")) {
        Ast *b = statement();
        token.skip(";");
        cont.push_back(b);
    }

    return new Node_block(cont);
}

Ast *Parser::make_if() {
    if(token.skip("if")) {
        token.skip("(");
        Ast *cond = expr();
        token.skip(")");
        Ast *then = statement();
        token.skip(";");

        if(token.skip("else")) {
            Ast *el = statement();

            return new Node_if(cond, then, el);
        }

        return new Node_if(cond, then, nullptr);
    }
    else
        return nullptr;
}

Ast *Parser::make_for() {
    if(token.skip("for")) {
        token.skip("(");
        Ast *init = expr();
        token.abs_skip(";");
        Ast *cond = expr();
        token.abs_skip(";");
        Ast *reinit = expr();
        token.abs_skip(")");
        Ast *body = statement();

        return new Node_for(init, cond, reinit, body);
    }
    return nullptr;
}

Ast *Parser::make_while() {
    if(token.skip("while")) {
        token.skip("(");
        Ast *cond = expr();
        token.skip(")");
        Ast *body = statement();

        return new Node_while(cond, body);
    }
    return nullptr;
}

Ast *Parser::make_return() {
    Node_return *r = new Node_return(expr_first());
    token.skip(";");
    return r;
}

Ast *Parser::expr_num(token_t token) {
    if(token.type != TOKEN_TYPE_NUM) {
        error("not a number");
    }
    return new Node_number(token.value);
}

Ast *Parser::expr_var(token_t tk) {
    return new Node_variable(tk.value);
}

Ast *Parser::expr_logic_or() {
    Ast *left = expr_logic_and();

    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("||")) {
            token.step();
            left = new Node_binop("||", left, expr_logic_and());
        }
        else
            return left;
    }
}

Ast *Parser::expr_logic_and() {
    Ast *left = expr_equality();

    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("&&")) {
            token.step();
            left = new Node_binop("&&", left, expr_equality());
        }
        else
            return left;
    }
}

Ast *Parser::expr_equality() {
    Ast *left = expr_comp();

    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("==")) {
            token.step();
            left = new Node_binop("==", left, expr_comp());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("!=")) {
            token.step();
            left = new Node_binop("!=", left, expr_comp());
        }
        else
            return left;
    }
}

Ast *Parser::expr_comp() {
    Ast *left = expr_add();

    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("<")) {
            token.step();
            left = new Node_binop("<", left, expr_add());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value(">")) {
            token.step();
            left = new Node_binop(">", left, expr_add());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("<=")) {
            token.step();
            left = new Node_binop("<=", left, expr_add());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value(">=")) {
            token.step();
            left = new Node_binop(">=", left, expr_add());
        }
        else
            return left;
    }
}

Ast *Parser::expr_add() {
    Ast *left = expr_mul();

    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("+")) {
            token.step();
            left = new Node_binop("+", left, expr_mul());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("-")) {
            token.step();
            left = new Node_binop("-", left, expr_mul());
        }
        else {
            return left;
        }
    }
}

Ast *Parser::expr_mul() {
    Ast *left = expr_unary();

    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("*")) {
            token.step();
            left = new Node_binop("*", left, expr_unary());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("/")) {
            token.step();
            left = new Node_binop("/", left, expr_unary());
        }
        else if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("%")) {
            token.step();
            left = new Node_binop("%", left, expr_unary());
        }
        else {
            return left;
        }
    }
}

Ast *Parser::expr_unary() {
    if(token.is_value("++") || token.is_value("--")) {
        std::string op = token.get().value;
        token.step();
        return new Node_unaop(op, expr_unary());
    }
    else
        return expr_unary_postfix();
}

Ast *Parser::expr_unary_postfix() {
    //TODO
    return expr_primary();
}

Ast *Parser::expr_primary() {
    while(1) {
        if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value("(")) {
            token.step();
            Ast *left = expr_first();

            if(token.is_type(TOKEN_TYPE_SYMBOL) && token.is_value(")")) {
                token.step();
                return left;
            }
            else {
                error(") <- not found");
            }
        }

        if(is_func_call())
            return func_call();
        else if(token.is_type(TOKEN_TYPE_IDENTIFER)) {
            return expr_var(token.get_step());
        }
        else if(token.is_type(TOKEN_TYPE_NUM))
            return expr_num(token.get_step());
        else if(token.is_value(";"))
            return nullptr;

        error("in expr_primary: ????");
        return nullptr;
    }
}

bool Parser::is_func_def() {
    if(token.is_type()) {
        token.save();
        token.step();
        token.step();

        if(token.skip("(")) {
            while(!token.is_value(")"))
                token.step();
            token.skip(")");
            if(token.skip("{")) {
                token.rewind();

                return true;
            }
        }
        token.rewind();

        return false;
    }
    else
        return false;
}

bool Parser::is_func_call() {
    if(token.is_type(TOKEN_TYPE_IDENTIFER)) {
        token.save();
        token.step();
        if(token.is_value("(")) {
            token.rewind();
            return true;
        }
        else {
            token.rewind();
            return false;
        }
    }
    else
        return false;
}

bool Parser::is_var_decl() {
    if(token.is_type()) {
        token.save();
        token.step();

        if(token.is_type(TOKEN_TYPE_IDENTIFER)) {
            token.rewind();

            return true;
        }
        token.rewind();

        return false;
    }
    else
        return false;
}

std::string Parser::show_type(var_type ty) {
    switch(ty) {
        case TYPE_INT:  return "int";
        case TYPE_VOID: return "void";
        default:        return "????";
    }
}

void Parser::show(Ast *ast) {
    if(ast != nullptr) {
        switch(ast->get_nd_type()) {
            case ND_TYPE_NUM: {
                Node_number *n = (Node_number *)ast;
                std::cout << n->number << " ";
                break;
            }
            case ND_TYPE_SYMBOL: {
                Node_binop *b = (Node_binop *)ast;
                printf("(");
                std::cout << b->symbol << " ";
                show(b->left);
                show(b->right);
                printf(")");
                break;
            }
            case ND_TYPE_VARDECL: {
                Node_var_decl *v = (Node_var_decl *)ast;
                printf("var_decl: ");
                for(auto decl: v->decl_v)
                    std::cout << "(" << show_type(decl.type) << ", " << decl.name << ")";
                break;
            }
            case ND_TYPE_ASSIGNMENT: {
                Node_assignment *a = (Node_assignment *)ast;
                printf("(= (");
                show(a->dst);
                printf(") (");
                show(a->src);
                printf("))");
                break;
            }
            case ND_TYPE_IF: {
                Node_if *i = (Node_if *)ast;
                printf("(if ");
                show(i->cond);
                printf("(");
                show(i->then_s);
                printf(")");
                if(i->else_s) {
                    printf("(else ");
                    show(i->else_s);
                    printf(")");
                }
                printf(")");
                break;
            }
            case ND_TYPE_WHILE: {
                Node_while *w = (Node_while *)ast;
                printf("(while ");
                show(w->cond);
                printf("(");
                show(w->body);
                printf("))");
                break;
            }
            case ND_TYPE_BLOCK: {
                Node_block *b = (Node_block *)ast;
                for(Ast *c: b->cont) {
                    show(c);
                    puts("");
                }
                break;
            }
            case ND_TYPE_RETURN: {
                Node_return *r = (Node_return *)ast;
                printf("return: ");
                show(r->cont);
                puts("");
                break;
            }
            case ND_TYPE_FUNCDEF: {
                Node_func_def *f = (Node_func_def *)ast;
                printf("func-def: (");
                std::cout << f->name << "(";
                for(auto a: f->args)
                    std::cout << "(" << show_type(a.type) << "," << a.name << ")";
                std::cout << ") -> " << show_type(f->ret_type) << "(" << std::endl;
                for(Ast *b: f->block) {
                    show(b);
                    puts("");
                }
                printf("))");
                break;
            }
            case ND_TYPE_FUNCCALL: {
                Node_func_call *f = (Node_func_call *)ast;
                printf("(func-call: (");
                std::cout << f->name << "(" << std::endl;
                for(Ast *a: f->arg_v) {
                    show(a);
                    puts("");
                }
                printf("))");
                break;
            }
            case ND_TYPE_VARIABLE: {
                Node_variable *v = (Node_variable *)ast;
                printf("(var: ");
                std::cout << v->name << ")";
                break;
            }
            default: {
                error("??????");
            }
        }
    }
}
