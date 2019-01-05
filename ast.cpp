#include"maxc.h"

void Ast::make(Token token) {
    pos = 0;

    node = expr_add(token.token_v);
    show();
}

ast_t *Ast::make_node(std::string value, ast_t *left, ast_t *right) {
    ast_t *new_node = new ast_t;

    new_node->type = [&]() -> nd_type {
        if(value == "+")
            return ND_TYPE_ADD;
        else if(value == "-")
            return ND_TYPE_SUB;
        else if(value == "*")
            return ND_TYPE_MUL;
        else if(value == "/")
            return ND_TYPE_DIV;
        else {
            fprintf(stderr, "make_node ???");
            exit(1);
        }
    }();

    new_node->left = left;
    new_node->right = right;

    return new_node;
}

ast_t *Ast::make_num_node(token_t token) {
    if(token.type != TOKEN_TYPE_NUM) {
        std::cerr << "This is not number: " << token.value << std::endl;;
        exit(1);
    }
    ast_t *new_node = new ast_t;

    new_node->type = ND_TYPE_NUM;
    new_node->value = token.value;
    new_node->left = nullptr;
    new_node->right = nullptr;

    return new_node;
}

ast_t *Ast::expr_add(std::vector<token_t> tokens) {
    ast_t *left = expr_mul(tokens);

    //print_pos("aaa");

    while(1) {
        if(tokens[pos].value == "+" && tokens[pos].type == TOKEN_TYPE_SYMBOL) {
            pos++;
            left = make_node("+", left, expr_mul(tokens));
        }
        else if(tokens[pos].value == "-" && tokens[pos].type == TOKEN_TYPE_SYMBOL) {
            pos++;
            left = make_node("-", left, expr_mul(tokens));
        }
        else {
            return left;
        }
    }
}

ast_t *Ast::expr_num(token_t token) {
    if(token.type != TOKEN_TYPE_NUM) {
        fprintf(stderr, "%s <- ???", token.value.c_str());
        exit(1);
    }
    return make_num_node(token);
}

ast_t *Ast::expr_mul(std::vector<token_t> tokens) {
    ast_t *left = expr_primary(tokens);

    while(1) {
        if(tokens[pos].type == TOKEN_TYPE_SYMBOL && tokens[pos].value == "*") {
            pos++;
            left = make_node("*", left, expr_primary(tokens));
        }
        else if(tokens[pos].type == TOKEN_TYPE_SYMBOL && tokens[pos].value == "/") {
            pos++;
            print_pos("uoa");
            left = make_node("/", left, expr_primary(tokens));
        }
        else {
            return left;
        }
    }
}

ast_t *Ast::expr_primary(std::vector<token_t> tokens) {
    while(1) {
        if(tokens[pos].type == TOKEN_TYPE_SYMBOL && tokens[pos].value == "(") {
            pos++;
            ast_t *left = expr_add(tokens);
            print_pos("ueaaa");

            if(tokens[pos].type != TOKEN_TYPE_SYMBOL || tokens[pos].value != ")") {
                fprintf(stderr, "[error] token: ')' was not found");
                exit(1);
            }

            return left;
        }

        if(tokens[pos].type == TOKEN_TYPE_NUM)
            return make_num_node(tokens[pos++]);

        fprintf(stderr, "expr_primary: ???");
        exit(1);
    }
}

void Ast::show() {
    if(node->type == ND_TYPE_NUM) {
        std::cout << node->value << std::endl;

        return ;
    }

    std::string type = ret_type(node->type);
    std::cout << type << std::endl;

    printf("left ");
    show(node->left);
    printf("right ");
    show(node->right);
}

void Ast::show(ast_t *current) {
    if(current->type == ND_TYPE_NUM) {
        std::cout << current->value << std::endl;
        return;
    }

    std::string type = ret_type(current->type);
    std::cout << type << std::endl;

    printf("left ");
    show(current->left);
    printf("right ");
    show(current->right);
}

void Ast::print_pos(std::string msg) {
    std::cout << msg << ": " << pos << std::endl;
}

std::string Ast::ret_type(nd_type ty) {
    switch(ty) {
        case ND_TYPE_ADD:
            return "+";
        case ND_TYPE_SUB:
            return "-";
        case ND_TYPE_MUL:
            return "*";
        case ND_TYPE_DIV:
            return "/";
        default:
            printf("???\n");
            exit(1);
    }
}
