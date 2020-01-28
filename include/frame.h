#ifndef MXC_FRAME_H
#define MXC_FRAME_H

#include "bytecode.h"
#include "function.h"
#include "maxc.h"
#include "util.h"
#include "error/runtime-err.h"

struct MxcObject;
typedef struct MxcObject MxcObject;

typedef struct Frame {
    struct Frame *prev;
    char *func_name;
    char *filename;
    uint8_t *code;
    void **label_ptr;
    const void **optab;
    size_t codesize;
    Varlist *lvar_info;
    MxcObject **lvars;
    MxcObject **gvars;
    size_t pc;
    size_t nlvars;
    size_t lineno;
    MxcObject **stackptr;
    RuntimeErr occurred_rterr;
} Frame;

Frame *New_Global_Frame(Bytecode *, int, Vector *);
Frame *New_Frame(userfunction *, Frame *);
Frame *New_DummyFrame();
void Delete_Frame(Frame *);

#endif
