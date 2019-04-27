#include "maxc.h"

#define Jmpcode() do{ ++pc; goto *codetable[(int)code[pc].type]; } while(0)

int VM::run(std::vector<vmcode_t> &code, std::map<const char *, int> &lmap) {
    if(!lmap.empty()) labelmap = lmap;
    if(code.empty()) return 1;
    env = new VMEnv();
    env->cur = new vmenv_t();
    exec(code);
    return 0;
}

void VM::exec(std::vector<vmcode_t> &code) {
    static const void *codetable[] = {
        &&code_push,
        &&code_ipush,
        &&code_pop,
        &&code_add,
        &&code_sub,
        &&code_mul,
        &&code_div,
        &&code_mod,
        &&code_logor,
        &&code_logand,
        &&code_eq,
        &&code_noteq,
        &&code_lt,
        &&code_lte,
        &&code_gt,
        &&code_gte,
        &&code_label,
        &&code_jmp,
        &&code_jmp_eq,
        &&code_jmp_noteq,
        &&code_inc,
        &&code_dec,
        &&code_print,
        &&code_print_int,
        &&code_print_char,
        &&code_print_str,
        &&code_println,
        &&code_println_int,
        &&code_println_char,
        &&code_println_str,
        &&code_format,
        &&code_typeof,
        &&code_load,
        &&code_store,
        &&code_istore,
        &&code_listset,
        &&code_stringset,
        &&code_tupleset,
        &&code_functionset,
        &&code_ret,
        &&code_call,
        &&code_callmethod,
        &&code_fnbegin,
        &&code_fnend,
        &&code_end,
    };

    goto *codetable[(int)code[pc].type];

code_push:
    {
        vmcode_t &c = code[pc];
        if(c.vtype == VALUE::Number) {
            int &_i = c.num;
            s.push(value_t(_i));
        }
        else if(c.vtype == VALUE::Char) {
            char &_c = c.ch;
            s.push(value_t(_c));
        }
        Jmpcode();
    }
code_ipush:
    stk.push(alloc_intobject(code[pc].num));
    Jmpcode();
code_pop:
    stk.pop();
    Jmpcode();
code_add:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_add(l, r));
        Jmpcode();
    }
code_sub:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_sub(l, r));
        Jmpcode();
    }
code_mul:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_mul(l, r));
        Jmpcode();
    }
code_div:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_div(l, r));
        Jmpcode();
    }
code_mod:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_mod(l, r));
        Jmpcode();
    }
code_logor:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_logor(l, r));
        Jmpcode();
    }
code_logand:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_logand(l, r));
        Jmpcode();
    }
code_eq:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_eq(l, r));
        Jmpcode();
    }
code_noteq:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_noteq(l, r));
        Jmpcode();
    }
code_lt:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_lt(l, r));
        Jmpcode();
    }
code_lte:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_lte(l, r));
        Jmpcode();
    }
code_gt:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_gt(l, r));
        Jmpcode();
    }
code_gte:
    {
        auto r = (IntObject *)stk.top(); stk.pop();
        auto l = (IntObject *)stk.top(); stk.pop();
        stk.push(int_gte(l, r));
        Jmpcode();
    }
code_inc:
    {
        auto u = (IntObject *)stk.top(); stk.pop();
        stk.push(int_inc(u));
        Jmpcode();
    }
code_dec:
    {
        auto u = (IntObject *)stk.top(); stk.pop();
        stk.push(int_dec(u));
        Jmpcode();
    }
code_store:
    {
        vmcode_t &c = code[pc];

        /*
        switch(c.var->var->ctype->get().type) {
            case CTYPE::INT:
                valstr = value_t(s.top().num); break;
            case CTYPE::CHAR:
                valstr = value_t(s.top().ch); break;
            case CTYPE::STRING:
                valstr = value_t(s.top().strob); break;
            case CTYPE::LIST:
                valstr = value_t(s.top().listob); break;
            default:
                runtime_err("unimplemented");
        }*/
        if(c.var->isglobal) {
            gvmap[c.var->vid] = stk.top();
        }
        else {
            env->cur->vmap[c.var->vid] = stk.top();
        }
        stk.pop();
        Jmpcode();
    }
code_istore:
    {
        vmcode_t &c = code[pc];
        if(c.var->isglobal)
            gvmap[c.var->vid] = stk.top();
        else
            env->cur->vmap[c.var->vid] = stk.top();
        stk.pop();
        Jmpcode();
    }
code_load:
    {
        vmcode_t &c = code[pc];
        if(c.var->isglobal)
            stk.push(gvmap.at(c.var->vid));
        else {
            //std::cout << c.var->var->vid << "\n";
            stk.push(env->cur->vmap.at(c.var->vid));
        }
        Jmpcode();
    }
code_print:
    if(s.empty()) runtime_err("stack is empty at %#x", pc);

    print(s.top());
    s.pop();
    Jmpcode();
code_print_int:
    {
        auto i = (IntObject *)stk.top();
        printf("%d", i->inum32); stk.pop();
        Jmpcode();
    }
code_print_char:
    printf("%c", s.top().ch); s.pop();
    Jmpcode();
code_print_str:
    {
        auto s = (StringObject *)stk.top();
        printf("%s", s->str); stk.pop();
        Jmpcode();
    }
code_println:
    if(s.empty()) runtime_err("stack is empty at %#x", pc);

    print(s.top()); puts("");
    s.pop();
    Jmpcode();
code_println_int:
    {
        auto i = (IntObject *)stk.top();
        printf("%d\n", i->inum32); stk.pop();
        Jmpcode();
    }
code_println_char:
    printf("%c\n", s.top().ch); s.pop();
    Jmpcode();
code_println_str:
    {
        auto s = (StringObject *)stk.top();
        printf("%s\n", s->str); stk.pop();
        Jmpcode();
    }
code_format:
    {
        Jmpcode();
    }
code_typeof:
    Jmpcode();
code_jmp:
    pc = labelmap[code[pc].str];
    Jmpcode();
code_jmp_eq:
    {
        auto a = (IntObject *)stk.top();
        if(a->inum32 == true)
            pc = labelmap[code[pc].str];
        stk.pop();
        Jmpcode();
    }
code_jmp_noteq:
    {
        auto a = (IntObject *)stk.top();
        if(a->inum32 == false)
            pc = labelmap[code[pc].str];
        stk.pop();
        Jmpcode();
    }
code_listset:
    {
        vmcode_t &c = code[pc];
        ListObject lob;
        for(lfcnt = 0; lfcnt < c.size; ++lfcnt) {
            lob.lselem.push_back(s.top()); s.pop();
        }
        s.push(value_t(lob));
        Jmpcode();
    }
code_stringset:
    {
        stk.push(alloc_stringobject(code[pc].str));
        Jmpcode();
    }
code_tupleset:
    {
        vmcode_t &c = code[pc];
        TupleObject tupob;
        for(lfcnt = 0; lfcnt < c.size; ++lfcnt) {
            tupob.tup.push_back(s.top()); s.pop();
        }
        s.push(value_t(tupob));
        Jmpcode();
    }
code_functionset:
    {
        stk.push(alloc_functionobject(code[pc].fnstart));
        Jmpcode();
    }
code_fnbegin:
    {
        for(;;) {
            ++pc;
            if(code[pc].type == OPCODE::FNEND && code[pc].str == code[pc].str) break;
        }
        Jmpcode();
    }
code_call:
    {
        //vmcode_t &c = code[pc];
        env->make();
        locs.push(pc);
        auto f = (FunctionObject *)stk.top();
        pc = f->start - 1; stk.pop();
        Jmpcode();
    }
code_callmethod:
    {
        vmcode_t &c = code[pc];
        switch(c.obmethod) {
            case Method::ListSize:
                {
                    cmlsob = s.top().listob; s.pop();
                    s.push(value_t((int)cmlsob.get_size()));
                } break;
            case Method::ListAccess:
                {
                    cmlsob = s.top().listob; s.pop();
                    int &index = s.top().num; s.pop();
                    s.push(value_t(cmlsob.lselem[index]));
                } break;
            case Method::StringLength:
                {
                    /*
                    cmstob = s.top().strob; s.pop();
                    s.push(value_t(cmstob.get_length()));
                    */
                } break;
            case Method::TupleAccess:
                {
                    cmtupob = s.top().tupleob; s.pop();
                    int &index = s.top().num; s.pop();
                    s.push(value_t(cmtupob.tup[index]));
                } break;
            default:
                error("unimplemented");
        }
        Jmpcode();
    }
code_ret:
    pc = locs.top(); locs.pop();
    env->escape();
    Jmpcode();
code_label:
code_fnend:
    Jmpcode();
code_end:
    return;
}

void VM::print(value_t &val) {
    ;
}

vmenv_t *VMEnv::make() {
    vmenv_t *e = new vmenv_t(cur);

    cur = e;
    return cur;
}

vmenv_t *VMEnv::escape() {
    vmenv_t *pe = cur->parent;
    //cur->vmap.clear();
    delete cur;
    cur = pe;
    return cur;
}
