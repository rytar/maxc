#include "maxc.h"

env_t *Env::make() {
    env_t *e = new env_t();
    current->nblock++;
    e->parent = current;
    e->isglobal = false;
    current->children.push_back(e);

    current = e;
    return current;
}

env_t *Env::escape() {
    if(!current->isglobal) {
        current = current->parent;
        return current;
    }

    return nullptr;
}

env_t *Env::get_cur() {
    return current;
}

env_t *Env::down(int n) {
    current = current->children[n];
    return current;
}

env_t *Env::up() {
    current = current->parent;
    return current;
}

void Varlist::push(var_t v) {
    var_v.push_back(v);
}

var_t *Varlist::find(std::string n) {
    for(var_t &v: var_v) {
        if(v.name == n) return &v;
    }

    return nullptr;
}

/*
void Funclist::push(func_t f) {
    funcs.push_back(f);
}

func_t *Funclist::find(std::string n) {
    for(auto &f: funcs) {
        if(f.name == n) return &f;
    }

    return nullptr;
}
*/
