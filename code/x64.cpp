
enum X64_Opcode {
    X64Opcode_noop, // does nothing
    X64Opcode_int3, // debug breakpoint
    X64Opcode_mov,  // op0 = op1
    X64Opcode_add,  // op0 += op1
    X64Opcode_sub,  // op0 -= op1
    X64Opcode_imul, // op0 *= op1
    X64Opcode_idiv, // op0 /= op1
};

const cstring x64_opcode_name_table[] = {
    "noop", "int3", "mov", "add", "sub", "mul", "div"
};

enum X64_Operand_Kind {
    X64Operand_None,
    X64Operand_r32,
    X64Operand_m32,
    X64Operand_imm32,
};

typedef int X64_Encoding;
enum  {
    X64Encoding_r = bit(X64Operand_r32),
    X64Encoding_rr = bit(X64Operand_r32) | bit(X64Operand_r32 + 3),
    X64Encoding_rm = bit(X64Operand_r32) | bit(X64Operand_m32 + 3),
    X64Encoding_mr = bit(X64Operand_m32) | bit(X64Operand_r32 + 3),
    X64Encoding_ri = bit(X64Operand_r32) | bit(X64Operand_imm32 + 3),
    X64Encoding_mi = bit(X64Operand_m32) | bit(X64Operand_imm32 + 3),
};

enum X64_Register {
    X64Register_None,
    X64Register_rax,
    X64Register_rcx,
    X64Register_rdx,
    X64Register_rbx,
    X64Register_rsp,
    X64Register_rbp,
    X64Register_rsi,
    X64Register_rdi,
};

const cstring x64_register_name_table[] = {
    "?", "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"
};

struct X64_Operand {
    X64_Operand_Kind kind;
    union {
        u32 reg;
        s32 imm;
    };
    s32 displacement;
    X64_Register reg_allocated;
    b32 is_allocated;
};

struct X64_Instruction {
    X64_Opcode opcode;
    X64_Operand op0;
    X64_Operand op1;
    X64_Encoding encoding;
};

struct X64_Builder {
    array(X64_Instruction)* instructions;
    map(u32, s32)* stack_offsets;
    s32 stack_pointer;
};

inline void
x64_push_instruction(X64_Builder* x64, X64_Instruction insn) {
    insn.encoding = bit(insn.op0.kind) | bit(insn.op1.kind);
    array_push(x64->instructions, insn);
}

X64_Operand
x64_build_operand(X64_Builder* x64, Bc_Operand operand) {
    X64_Operand result = {};
    
    switch (operand.kind) {
        case BcOperand_Register: {
            if (operand.type == BcType_s32_ptr) {
                result.kind = X64Operand_m32;
                result.reg = 0;
                result.reg_allocated = X64Register_rsp;
                result.displacement = map_get(x64->stack_offsets, operand.Register);
                result.is_allocated = true;
            } else {
                result.kind = X64Operand_r32;
                result.reg = operand.Register;
            }
        } break;
        
        case BcOperand_Int: {
            result.kind = X64Operand_imm32;
            result.imm = operand.Signed_Int;
        } break;
        
        default: {
            assert(0 && "cannot convert bytecode operand");
        } break;
    }
    
    return result;
}

void
x64_push_instruction(X64_Builder* x64, X64_Opcode opcode, Bc_Operand op0, Bc_Operand op1) {
    X64_Instruction insn = {};
    insn.opcode = opcode;
    insn.op0 = x64_build_operand(x64, op0);
    insn.op1 = x64_build_operand(x64, op1);
    x64_push_instruction(x64, insn);
}

void
convert_to_x64_instruction(X64_Builder* x64, Bc_Instruction* bc) {
    switch (bc->opcode) {
        case Bytecode_push: { // dest = push src0
            map_put(x64->stack_offsets, bc->dest.Register, x64->stack_pointer);
            x64->stack_pointer -= bc->src0.Signed_Int;
        } break;
        
        case Bytecode_store: { // *src0 = src1 -> mov [src0], src1
            x64_push_instruction(x64, X64Opcode_mov, bc->src0, bc->src1);
        } break;
        
        case Bytecode_load: {   // dest = *src0 -> mov dest, [src0]
            x64_push_instruction(x64, X64Opcode_mov, bc->dest, bc->src0);
        } break;
        
        case Bytecode_add: { // dest = src1; dest += src2
            x64_push_instruction(x64, X64Opcode_mov, bc->dest, bc->src0);
            x64_push_instruction(x64, X64Opcode_add, bc->dest, bc->src1);
        } break;
        
        case Bytecode_sub: { // dest = src1; dest -= src2
            x64_push_instruction(x64, X64Opcode_mov, bc->dest, bc->src0);
            x64_push_instruction(x64, X64Opcode_add, bc->dest, bc->src1);
        } break;
        
        case Bytecode_mul: { // dest = src1; dest *= src2
            x64_push_instruction(x64, X64Opcode_mov, bc->dest, bc->src0);
            x64_push_instruction(x64, X64Opcode_imul, bc->dest, bc->src1);
        } break;
    }
}

void
convert_to_x64(X64_Builder* x64, array(Bc_Instruction)* instructions) {
    for_array(instructions, insn, _) {
        convert_to_x64_instruction(x64, insn);
    }
}


struct Machine_Code {
    u8* data;
    umm count;
};

Machine_Code
assemble_x64_instruction_to_machine_code(X64_Builder* x64, X64_Instruction* insn) {
    Machine_Code result = {};
    result.data = (u8*) malloc(1024);
    u8* curr = result.data;
    
    
    // opcode
    switch (insn->opcode) {
        case X64Opcode_mov: {
            switch (insn->encoding) {
                case X64Encoding_rr:
                case X64Encoding_mr: *curr++ = 0x89; break;
                case X64Encoding_rm: *curr++ = 0x8B; break;
                case X64Encoding_ri:
                case X64Encoding_mi: *curr++ = 0xC7; break;
                default: assert(0 && "invald operands for MOV"); break;
            }
        } break;
        
        case X64Opcode_add: {
            switch (insn->encoding) {
                case X64Encoding_rr:
                case X64Encoding_mr: *curr++ = 0x01; break;
                case X64Encoding_rm: *curr++ = 0x03; break;
                case X64Encoding_ri:
                case X64Encoding_mi: *curr++ = 0x81; break;
                default: assert(0 && "invald operands for ADD"); break;
            }
        } break;
        
        case X64Opcode_sub: {
            switch (insn->encoding) {
                case X64Encoding_rr:
                case X64Encoding_mr: *curr++ = 0x29; break;
                case X64Encoding_rm: *curr++ = 0x2B; break;
                case X64Encoding_ri:
                case X64Encoding_mi: *curr++ = 0x81; break;
                default: assert(0 && "invald operands for SUB"); break;
            } break;
        } break;
        
        case X64Opcode_imul: {
            switch (insn->encoding) {
                case X64Encoding_rr: 
                case X64Encoding_rm: *curr++ = 0x0F; *curr++ = 0xAF; break;
                default: assert(0 && "invald operands for IMUL"); break;
            }
        } break;
        
        case X64Opcode_idiv: {
            switch (insn->encoding) {
                case X64Encoding_r: *curr++ = 0xF7; break;
                default: assert(0 && "invald operands for IDIV"); break;
            }
        } break;
    }
    
    // modrm
    
    
    if (insn->encoding == X64Encoding_mr ||
        insn->encoding == X64Encoding_rm ||
        insn->encoding == X64Encoding_rm) {
        u8* disp_bytes = (u8*) &insn.displacement;
        for (s32 byte_index = 0; byte_index < sizeof(s32); byte_index++) {
            // TODO(Alexander): little-endian
            push_u8(assembler, disp_bytes[byte_index]);
        }
    }
    
    result.count = (umm) result.data - (umm) result.data;
    return result;
}

void
string_builder_push(String_Builder* sb, X64_Operand* operand, bool show_virtual_registers=false) {
    switch (operand->kind) {
        case X64Operand_r32: {
            if (operand->is_allocated) {
                X64_Register actual_reg = operand->reg_allocated;
                string_builder_push_format(sb, "%", f_cstring(x64_register_name_table[actual_reg]));
            } else {
                string_builder_push_format(sb, "r%", f_u32(operand->reg));
            }
        } break;
        
        case X64Operand_m32: {
            string_builder_push(sb, "dword ptr ");
            if (operand->is_allocated) {
                X64_Register actual_reg = operand->reg_allocated;
                string_builder_push_format(sb, "[%", f_cstring(x64_register_name_table[actual_reg]));
            } else {
                string_builder_push_format(sb, "[r%", f_u32(operand->reg));
            }
            
            if (operand->displacement > 0) {
                string_builder_push_format(sb, " + %", f_u64(operand->displacement));
            } else if (operand->displacement < 0) {
                string_builder_push_format(sb, " - %", f_u64(-operand->displacement));
            }
            
            string_builder_push(sb, "]");
        } break;
        
        case X64Operand_imm32: {
            string_builder_push_format(sb, "%", f_s64(operand->imm));
        } break;
    }
}

void
string_builder_push(String_Builder* sb, X64_Instruction* insn, bool show_virtual_registers=false) {
    cstring mnemonic = x64_opcode_name_table[insn->opcode];
    
    string_builder_push_format(sb, "%", f_cstring(mnemonic));
    if (insn->op0.kind) {
        string_builder_push(sb, " ");
        string_builder_push(sb, &insn->op0, show_virtual_registers);
    }
    
    if (insn->op1.kind) {
        string_builder_push(sb, ", ");
        string_builder_push(sb, &insn->op1, show_virtual_registers);
    }
}

void
string_builder_push(String_Builder* sb, X64_Builder* x64) {
    for_array(x64->instructions, insn, _) {
        string_builder_push(sb, insn);
        string_builder_push(sb, "\n");
    }
}

void
x64_print_program(X64_Builder* x64) {
    String_Builder sb = {};
    for_array(x64->instructions, insn, _) {
        string_builder_push(&sb, insn);
        string_builder_push(&sb, "\n");
    }
    string result = string_builder_to_string_nocopy(&sb);
    pln("%", f_string(result));
    string_builder_free(&sb);
}
