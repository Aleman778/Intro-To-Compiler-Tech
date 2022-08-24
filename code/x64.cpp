
enum X64_Opcode {
    X64Opcode_noop, // does nothing
    X64Opcode_int3, // debug breakpoint
    X64Opcode_mov,  // op0 = op1
    X64Opcode_add,  // op0 += op1
    X64Opcode_sub,  // op0 -= op1
    X64Opcode_imul, // op0 *= op1
    X64Opcode_idiv, // op0 /= op1
    X64Opcode_ret,  // returns RAX (on windows)
};

const cstring x64_opcode_name_table[] = {
    "noop", "int3", "mov", "add", "sub", "mul", "div", "ret"
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
    X64Encoding_m = bit(X64Operand_m32),
    X64Encoding_rr = bit(X64Operand_r32) | bit(X64Operand_r32 + 3),
    X64Encoding_rm = bit(X64Operand_r32) | bit(X64Operand_m32 + 3),
    X64Encoding_mr = bit(X64Operand_m32) | bit(X64Operand_r32 + 3),
    X64Encoding_ri = bit(X64Operand_r32) | bit(X64Operand_imm32 + 3),
    X64Encoding_mi = bit(X64Operand_m32) | bit(X64Operand_imm32 + 3),
};

enum X64_Register {
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
    "eax", "ecx", "edx", "ebx", "rsp", "rbp", "esi", "edi"
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
    insn.encoding = bit(insn.op0.kind) | bit(insn.op1.kind + 3);
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
                result.reg_allocated = X64Register_rbp;
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
            x64->stack_pointer -= bc->src0.Signed_Int;
            map_put(x64->stack_offsets, bc->dest.Register, x64->stack_pointer);
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
        
        case Bytecode_ret: { // ret src0;
            X64_Operand rax = {};
            rax.kind = X64Operand_r32;
            rax.reg_allocated = X64Register_rax;
            rax.is_allocated = true;
            
            X64_Instruction mov_insn = {};
            mov_insn.opcode = X64Opcode_mov;
            mov_insn.op0 = rax;
            mov_insn.op1 = x64_build_operand(x64, bc->src0);
            x64_push_instruction(x64, mov_insn);
            
            X64_Instruction ret_insn = {};
            ret_insn.opcode = X64Opcode_ret;
            x64_push_instruction(x64, ret_insn);
        } break;
    }
}

void
convert_to_x64(X64_Builder* x64, array(Bc_Instruction)* instructions) {
    // simple prologue, mov rbp, rsp (windows doesn't use rbp)
    X64_Operand rbp = {};
    rbp.kind = X64Operand_r32;
    rbp.reg_allocated = X64Register_rbp;
    rbp.is_allocated = true;
    
    X64_Operand rsp = {};
    rsp.kind = X64Operand_r32;
    rsp.reg_allocated = X64Register_rsp;
    rsp.is_allocated = true;
    
    X64_Instruction mov_insn = {};
    mov_insn.opcode = X64Opcode_mov;
    mov_insn.op0 = rbp;
    mov_insn.op1 = rsp;
    x64_push_instruction(x64, mov_insn);
    
    for_array(instructions, insn, _) {
        convert_to_x64_instruction(x64, insn);
    }
}


void
allocate_x64_registers(array(X64_Instruction)* instructions) {
    X64_Register free_regs[] = {
        X64Register_rdi, X64Register_rsi, X64Register_rbx, 
        X64Register_rdx, X64Register_rcx, X64Register_rax
    };
    int free_count = fixed_array_count(free_regs);
    map(u32, X64_Register)* allocated_regs = 0;
    
    for (int i = 0; i < array_count(instructions); i++) {
        X64_Instruction* curr = instructions + i;
        
        if (curr->op0.kind == X64Operand_r32 && !curr->op0.is_allocated) {
            smm index = map_get_index(allocated_regs, curr->op0.reg);
            if (index != -1) {
                // Used in destination, it's fine just allow it
                curr->op0.reg_allocated = allocated_regs[index].value;
                curr->op0.is_allocated = true;
            } else {
                // Allocate
                assert(free_count > 0 && "ran out of registers");
                curr->op0.reg_allocated = free_regs[--free_count];
                curr->op0.is_allocated = true;
                map_put(allocated_regs, curr->op0.reg, curr->op0.reg_allocated);
            }
        }
        
        if (curr->op1.kind == X64Operand_r32 && !curr->op1.is_allocated) {
            // Free after use
            assert(free_count < fixed_array_count(free_reg));
            curr->op1.reg_allocated = map_get(allocated_regs, curr->op1.reg);
            curr->op1.is_allocated = true;
            free_regs[free_count++] = curr->op1.reg_allocated;
        }
    }
}


struct Machine_Code {
    u8* bytes;
    umm size;
};

void
assemble_x64_instruction_to_machine_code(Machine_Code* code, X64_Instruction* insn) {
    u8* curr = code->bytes + code->size;
    
    if (insn->opcode == X64Opcode_ret) {
        *curr++ = 0xC3; // near return
        code->size++;
        return;
    }
    
    // REX
    if (insn->op1.reg_allocated == X64Register_rsp) {
        *curr++ = 0b01001000;
    }
    
    // opcode
    u8 reg = 0;
    switch (insn->opcode) {
        case X64Opcode_mov: {
            switch (insn->encoding) {
                case X64Encoding_mr: *curr++ = 0x89; break;
                case X64Encoding_rr:
                case X64Encoding_rm: *curr++ = 0x8B; break;
                case X64Encoding_ri:
                case X64Encoding_mi: *curr++ = 0xC7; reg = 0; break;
                default: assert(0 && "invald operands for MOV"); break;
            }
        } break;
        
        case X64Opcode_add: {
            switch (insn->encoding) {
                case X64Encoding_mr: *curr++ = 0x01; break;
                case X64Encoding_rr:
                case X64Encoding_rm: *curr++ = 0x03; break;
                case X64Encoding_ri:
                case X64Encoding_mi: *curr++ = 0x81; reg = 0; break;
                default: assert(0 && "invald operands for ADD"); break;
            }
        } break;
        
        case X64Opcode_sub: {
            switch (insn->encoding) {
                case X64Encoding_mr: *curr++ = 0x29; break;
                case X64Encoding_rr:
                case X64Encoding_rm: *curr++ = 0x2B; break;
                case X64Encoding_ri:
                case X64Encoding_mi: *curr++ = 0x81; reg = 5; break;
                default: assert(0 && "invald operands for SUB"); break;
            } break;
        } break;
        
        case X64Opcode_imul: {
            switch (insn->encoding) {
                case X64Encoding_rr: 
                case X64Encoding_rm: *curr++ = 0x0F; *curr++ = 0xAF; reg = 5; break;
                default: assert(0 && "invald operands for IMUL"); break;
            }
        } break;
        
        case X64Opcode_idiv: {
            switch (insn->encoding) {
                case X64Encoding_m:
                case X64Encoding_r: *curr++ = 0xF7; reg = 7; break;
                default: assert(0 && "invald operands for IDIV"); break;
            }
        } break;
    }
    
    // modrm
    u8 modrm_mod = 0;
    u8 modrm_reg = reg;
    u8 modrm_rm = 0;
    
    if (insn->encoding == X64Encoding_r || 
        insn->encoding == X64Encoding_rr || 
        insn->encoding == X64Encoding_ri) {
        modrm_mod = 0b11; // direct addressing
    } else {
        modrm_mod = 0b10; // indirect addressing
    }
    
    switch (insn->encoding) {
        case X64Encoding_ri:
        case X64Encoding_mi:
        case X64Encoding_r: {
            modrm_rm = insn->op0.reg_allocated;
        } break;
        
        case X64Encoding_m: {
            modrm_rm = insn->op0.reg_allocated;
        } break;
        
        case X64Encoding_mr: {
            modrm_rm = insn->op0.reg_allocated;
            modrm_reg = insn->op1.reg_allocated;
        } break;
        
        case X64Encoding_rr:
        case X64Encoding_rm: {
            modrm_reg = insn->op0.reg_allocated;
            modrm_rm = insn->op1.reg_allocated;
        } break;
    }
    
    *curr++ = (modrm_mod << 6) | (modrm_reg << 3) | modrm_rm;
    
    // displacement
    u8* disp_bytes = 0;
    if (insn->encoding == X64Encoding_mr || insn->encoding == X64Encoding_mi) {
        disp_bytes = (u8*) &insn->op0.displacement;
    } else if (insn->encoding == X64Encoding_rm) {
        disp_bytes = (u8*) &insn->op1.displacement;
    }
    if (disp_bytes) {
        for (s32 byte_index = 0; byte_index < sizeof(s32); byte_index++) {
            // TODO(Alexander): little-endian
            *curr++ = disp_bytes[byte_index];
        }
    }
    
    // immediate
    if (insn->encoding == X64Encoding_ri || 
        insn->encoding == X64Encoding_mi) {
        u8* imm_bytes = (u8*) &insn->op1.imm;
        for (s32 byte_index = 0; byte_index < sizeof(s32); byte_index++) {
            // TODO(Alexander): little-endian
            *curr++ = imm_bytes[byte_index];
        }
    }
    
    code->size = (umm) curr - (umm) code->bytes;
}


Machine_Code
assemble_to_x64_machine_code(array(X64_Instruction)* instructions) {
    Machine_Code code = {};
    code.bytes = (u8*) malloc(1024); // playing a bit unsafe, we can overflow this
    
    // int3 breakpoint
    //*code.bytes = 0xCC;
    //code.size++;
    
    for_array(instructions, insn, _) {
        assemble_x64_instruction_to_machine_code(&code, insn);
    }
    
    return code;
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
    
    switch (insn->encoding) {
        case X64Encoding_r: string_builder_push(sb, "(R)  "); break;
        case X64Encoding_rr: string_builder_push(sb, "(RR) "); break;
        case X64Encoding_mr: string_builder_push(sb, "(MR) "); break;
        case X64Encoding_rm: string_builder_push(sb, "(RM) "); break;
        case X64Encoding_ri: string_builder_push(sb, "(RI) "); break;
        case X64Encoding_mi: string_builder_push(sb, "(MI) "); break;
        default: string_builder_push(sb, "(??) "); break;
    }
    
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
