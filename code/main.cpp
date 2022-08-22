#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "basic.h"

#include "tokenizer.cpp"
#include "parser.cpp"
#include "interp.cpp"
#include "bytecode.cpp"
#include "x64.cpp"

#if BUILD_WINDOWS
#include <windows.h>
#endif


string
read_entire_file(cstring filepath) {
    string result = {};
    
    FILE* file;
    fopen_s(&file, filepath, "rb");
    if (!file) {
        pln("File `%` was not found!", f_cstring(filepath));
        return result;
    }
    
    fseek(file, 0, SEEK_END);
    umm file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    
    result.data = (u8*) malloc(file_size);
    result.count = file_size;
    fread(result.data, result.count, 1, file);
    fclose(file);
    return result;
}

Ast*
parse_source(string source) {
    // Tokenizer
    Tokenizer tokenizer = {};
    tokenizer.start = source.data;
    tokenizer.end = tokenizer.start + source.count;
    tokenizer.curr = tokenizer.start;
    
    // Parser
    Parser parser = {};
    parser.tokenizer = &tokenizer;
    Ast* ast = parse_block(&parser);
    
    return ast;
}

char*
getline() {
    // From https://stackoverflow.com/questions/314401/how-to-read-a-line-from-the-console-in-c
    char* line = (char*) malloc(100), * linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;
    
    if(line == NULL)
        return NULL;
    
    for(;;) {
        c = fgetc(stdin);
        if(c == EOF)
            break;
        
        if(--len == 0) {
            len = lenmax;
            char* linen = (char*) realloc(linep, lenmax *= 2);
            
            if(linen == NULL) {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }
        
        if((*line++ = c) == '\n')
            break;
    }
    *line = '\0';
    return linep;
}

typedef int asm_main(void);

int
main(int argc, char** argv) {
    
    if (argc >= 2) {
        string source = read_entire_file(argv[1]);
        Ast* ast = parse_source(source);
        
        // Interpreter
        Interp interp = {};
        Interp_Scope scope = {};
        array_push(interp.scopes, scope);
        
        Value interp_result = interp_expression(&interp, ast);
        pln("Interpreter exited with %\n", f_int(interp_result.integer));
        
        // Bytecode builder
        Bc_Builder bc_builder = {};
        bc_build_expression(&bc_builder, ast);
        bc_print_program(&bc_builder);
        
        // X64 assembler
        X64_Builder x64_builder = {};
        convert_to_x64(&x64_builder, bc_builder.instructions);
        x64_print_program(&x64_builder);
        
        Machine_Code code = assemble_to_x64_machine_code(x64_builder.instructions);
        pln("\nX64 Machine Code (% bytes):", f_umm(code.size));
        for (int byte_index = 0; byte_index < code.size; byte_index++) {
            u8 byte = code.bytes[byte_index];
            if (byte > 0xF) {
                printf("%hhX ", byte);
            } else {
                printf("0%hhX ", byte);
            }
            
            if (byte_index % 16 == 15) {
                printf("\n");
            }
        }
        
        // Run the code JIT
#if BUILD_WINDOWS
        u32 asm_buffer_size = code.size + 1024;
        void* asm_buffer = VirtualAlloc(0, asm_buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        memcpy(asm_buffer, code.bytes, code.size);
        DWORD prev_protect = 0;
        VirtualProtect(asm_buffer, asm_buffer_size, PAGE_EXECUTE_READ, &prev_protect);
        asm_main* func = (asm_main*) asm_buffer;
        int jit_exit_code = (int) func();
#endif
        
        
        
        
    } else {
        
        // Run interpreter in a REPL
        Interp interp = {};
        Interp_Scope scope = {};
        array_push(interp.scopes, scope);
        
        for (;;) {
            printf("> ");
            
            char* input = getline();
            int count = strlen(input);
            string source = create_string(count, (u8*) input);
            if (string_equals(source, string_lit("exit\n"))) {
                pln("bye bye...");
                break;
            }
            
            Ast* ast = parse_source(source);
            Value interp_result = interp_expression(&interp, ast);
            if (interp_result.type == Value_integer) {
                pln("= %", f_int(interp_result.integer));
            }
            
            printf("\n");
            free(input);
        }
    }
}

