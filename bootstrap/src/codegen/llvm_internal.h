// State shared by the LLVM IR generator files.
//
// Each AIR local is an `alloca` in the entry block (`%l<N>`); LLVM's mem2reg
// promotes them to registers. SSA temporaries are `%t<N>`. String literals,
// source locations and intrinsic declarations are collected while the
// functions are generated and written after them.

#ifndef ASTRA_LLVM_INTERNAL_H
#define ASTRA_LLVM_INTERNAL_H

#include "codegen/llvm.h"
#include "codegen/out.h"

typedef struct {
    Sema *sema;
    const TypeTable *types;
    Out out;
    Out globals;             // string literals and locations, written at the end
    uint32_t str_count, loc_count;
    uint64_t overflow_used[2][3][4];   // [signed][add, sub, mul][i8, i16, i32, i64]
    const AirBody *body;     // the function being generated
    uint32_t temp;           // next %t number in this function
} Gen;

// A generated value: its LLVM type and its text (`%t3`, `42`, `true`, ...).
typedef struct {
    const char *type;
    char text[160];
} Value;

const char *llvm_type(Gen *g, TypeId type);
bool llvm_has_value(TypeId type);   // false for () and !
StrView def_name(Gen *g, DefId def);
void function_name(Gen *g, Out *out, DefId def, bool is_main);

// A fresh temporary name, written into `text`.
void next_temp(Gen *g, char text[16]);

uint32_t string_global(Gen *g, StrView text);   // the N of @.str.N
uint32_t location_global(Gen *g, Span span);     // the N of @.loc.N

// llvm_body.c
void gen_body(Gen *g, const AirBody *body);

// llvm_ops.c
Value gen_operand(Gen *g, Operand o);
void gen_rvalue(Gen *g, const Rvalue *rv, TypeId dest_type, Value *result);
void gen_cast_fails(Gen *g, const Rvalue *rv, Value *result);

#endif
