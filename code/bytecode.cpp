
enum Bc_Opcode {
    Bytecode_noop,  // does nothing
    Bytecode_push,  // dest = (s32*) SP; SP -= src0
    Bytecode_load,  // dest = *src0
    Bytecode_store, // *dest = src0
    Bytecode_add,   // dest = src0 + src1
    Bytecode_sub,   // dest = src0 - src1
    Bytecode_mul,   // dest = src0 * src1
    Bytecode_div,   // dest = src0 / src1
    Bytecode_ret,   // returns src0
    // ... 
};

const cstring opcode_names[] = {
    "noop", "push", "load", "store", "add", "sub", "mul", "div", "ret"
};

enum Bc_Type {
    BcType_s32,
    BcType_s32_ptr,
};

enum Bc_Operand_Kind {
    BcOperand_None,
    BcOperand_Register,
    BcOperand_Int,
};

struct Bc_Operand {
    Bc_Operand_Kind kind;
    Bc_Type type;
    union {
        u32 Register;
        s32 Signed_Int;
    };
};

struct Bc_Instruction {
    Bc_Opcode opcode;
    Bc_Operand dest;
    Bc_Operand src0;
    Bc_Operand src1;
};

Bc_Instruction
create_test_bc_instruction() {
    // s32 r0 = add r1, 20
    
    Bc_Instruction insn = {};
    insn.opcode = Bytecode_add;
    
    insn.dest.type = BcType_s32;
    insn.dest.kind = BcOperand_Register;
    insn.dest.Register = 0;
    
    insn.src0.type = BcType_s32;
    insn.src0.kind = BcOperand_Register;
    insn.src0.Register = 1;
    
    insn.src1.type = BcType_s32;
    insn.src1.kind = BcOperand_Int;
    insn.src1.Signed_Int = 20;
    return insn;
}

struct Bc_Builder {
    array(Bc_Instruction)* instructions;
    map(string_id, Bc_Operand)* locals;
    u32 next_free_register;
};

Bc_Operand
bc_unique_register(Bc_Builder* bc, Bc_Type type) {
    Bc_Operand result = {};
    result.kind = BcOperand_Register;
    result.type = type;
    result.Register = bc->next_free_register++;
    return result;
}

void
bc_push(Bc_Builder* bc, Bc_Operand dest, s32 size) {
    Bc_Instruction insn = {};
    insn.dest = dest;
    insn.opcode = Bytecode_push;
    insn.src0.kind = BcOperand_Int;
    insn.src0.type = BcType_s32_ptr;
    insn.src0.Signed_Int = sizeof(s32);
    array_push(bc->instructions, insn);
}

Bc_Operand
bc_load(Bc_Builder* bc, Bc_Operand operand) {
    if (operand.type == BcType_s32) {
        return operand;
    }
    
    Bc_Instruction insn = {};
    insn.dest = bc_unique_register(bc, BcType_s32);
    insn.opcode = Bytecode_load;
    insn.src0 = operand;
    array_push(bc->instructions, insn);
    
    return insn.dest;
}

void
bc_ret(Bc_Builder* bc, Bc_Operand operand) {
    Bc_Instruction insn = {};
    insn.dest = bc_unique_register(bc, BcType_s32);
    insn.opcode = Bytecode_ret;
    insn.src0 = operand;
    array_push(bc->instructions, insn);
}

void
bc_store(Bc_Builder* bc, Bc_Operand dest, Bc_Operand src) {
    Bc_Instruction insn = {};
    insn.opcode = Bytecode_store;
    insn.src0 = dest;
    insn.src1 = src;
    array_push(bc->instructions, insn);
}

Bc_Operand
bc_binary(Bc_Builder* bc, Bc_Opcode opcode, Bc_Operand lhs, Bc_Operand rhs) {
    Bc_Instruction insn = {};
    insn.dest = bc_unique_register(bc, BcType_s32);
    insn.opcode = opcode;
    insn.src0 = lhs;
    insn.src1 = rhs;
    array_push(bc->instructions, insn);
    
    return insn.dest;
}

Bc_Operand
bc_build_expression(Bc_Builder* bc, Ast* node) {
    Bc_Operand result = {};
    
    switch (node->kind) {
        case Ast_Ident: {
            result = map_get(bc->locals, node->Ident);
            if (result.kind == BcOperand_None) {
                result = bc_unique_register(bc, BcType_s32_ptr);
                bc_push(bc, result, sizeof(s32));
                map_put(bc->locals, node->Ident, result);
            }
        } break;
        
        case Ast_Value: {
            result.kind = BcOperand_Int;
            result.type = BcType_s32;
            result.Signed_Int = node->Value.integer;
        } break;
        
        case Ast_Binary: {
            Bc_Operand lhs = bc_build_expression(bc, node->Binary.lhs);
            Bc_Operand rhs = bc_build_expression(bc, node->Binary.rhs);
            
            if (node->Binary.op == Binop_Assign) {
                rhs = bc_load(bc, rhs);
                bc_store(bc, lhs, rhs);
            } else {
                lhs = bc_load(bc, lhs);
                rhs = bc_load(bc, rhs);
                switch (node->Binary.op) {
#define BINARY_INT_CASE(binop, opcode) \
case Binop_##binop: { \
result = bc_binary(bc, Bytecode_##opcode, lhs, rhs); \
} break
                    
                    BINARY_INT_CASE(Add, add);
                    BINARY_INT_CASE(Sub, sub);
                    BINARY_INT_CASE(Mul, mul);
                    BINARY_INT_CASE(Div, div);
#undef BINARY_INT_CASE
                }
            }
        } break;
        
        case Ast_Block: {
            Bc_Operand result = {};
            for_array_v(node->Block.exprs, expr, _) {
                result= bc_build_expression(bc, expr);
            }
            if (result.kind != BcOperand_None) {
                bc_ret(bc, result);
            }
            
        } break;
    }
    
    return result;
}

bool
string_builder_push(String_Builder* sb, Bc_Operand* operand) {
    if (operand->kind == BcOperand_None) {
        return false;
    }
    
    switch (operand->type) {
        case BcType_s32: string_builder_push(sb, "s32 "); break;
        case BcType_s32_ptr: string_builder_push(sb, "s32* "); break;
    }
    
    switch (operand->kind) {
        case BcOperand_Register: {
            string_builder_push_format(sb, "r%", f_u32(operand->Register)); 
        } break;
        
        case BcOperand_Int: {
            string_builder_push_format(sb, "%", f_int(operand->Signed_Int));
        } break;
    }
    
    return true;
}

void
string_builder_push(String_Builder* sb, Bc_Instruction* insn) {
    if (string_builder_push(sb, &insn->dest)) {
        string_builder_push(sb, " = ");
    }
    string_builder_push(sb, opcode_names[insn->opcode]);
    string_builder_push(sb, " ");
    if (string_builder_push(sb, &insn->src0) &&
        insn->src1.kind != BcOperand_None) {
        string_builder_push(sb, ", ");
    }
    string_builder_push(sb, &insn->src1);
}

void
bc_print_program(Bc_Builder* bc) {
    String_Builder sb = {};
    for_array(bc->instructions, insn, _) {
        string_builder_push(&sb, insn);
        string_builder_push(&sb, "\n");
    }
    string result = string_builder_to_string_nocopy(&sb);
    pln("%", f_string(result));
    string_builder_free(&sb);
}
