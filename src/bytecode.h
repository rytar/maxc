#ifndef MAXC_BYTECODE_H
#define MAXC_BYTECODE_H

#include "maxc.h"

enum class BltinFnKind;

typedef std::vector<uint8_t> bytecode;

class LiteralPool;

enum class OpCode : uint8_t {
    END,
    PUSH,
    IPUSH,
    PUSHCONST_0,
    PUSHCONST_1,
    PUSHCONST_2,
    PUSHCONST_3,
    PUSHTRUE,
    PUSHFALSE,
    FPUSH,
    POP,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    LOGOR,
    LOGAND,
    EQ,
    NOTEQ,
    LT,
    LTE,
    GT,
    GTE,
    // float
    FADD,
    FSUB,
    FMUL,
    FDIV,
    FMOD,
    FLOGOR,
    FLOGAND,
    FEQ,
    FNOTEQ,
    FLT,
    FLTE,
    FGT,
    FGTE,
    JMP,
    JMP_EQ,
    JMP_NOTEQ,
    INC,
    DEC,
    FORMAT,
    TYPEOF,
    LOAD_GLOBAL,
    LOAD_LOCAL,
    STORE_GLOBAL,
    STORE_LOCAL,
    LISTSET,
    SUBSCR,
    SUBSCR_STORE,
    STRINGSET,
    TUPLESET,
    FUNCTIONSET,
    BLTINFN_SET,
    STRUCTSET,
    RET,
    CALL,
    CALL_BLTIN,
    MEMBER_LOAD,
    MEMBER_STORE,
};

void push_0arg(bytecode &, OpCode);
void push_ipush(bytecode &, int32_t);
void push_jmpneq(bytecode &, size_t);
void push_jmp(bytecode &, size_t);
void push_store(bytecode &, int, bool);
void push_load(bytecode &, int, bool);
void push_strset(bytecode &, int);
void push_fpush(bytecode &, int);
void push_functionset(bytecode &, int);
void push_bltinfn_set(bytecode &, BltinFnKind);
void push_structset(bytecode &, int);
void push_bltinfn_call(bytecode &, int);
void push_member_load(bytecode &, int);
void push_member_store(bytecode &, int);

void replace_int32(size_t, bytecode &, size_t);
void push_int8(bytecode &, int8_t);
void push_int32(bytecode &, int32_t);
int32_t read_int32(uint8_t[], size_t &);

#ifdef MXC_DEBUG
void codedump(uint8_t[], size_t &, LiteralPool &);
#endif

#endif
