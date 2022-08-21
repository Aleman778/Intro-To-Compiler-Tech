#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "basic.h"

#include "tokenizer.cpp"
#include "parser.cpp"
#include "interp.cpp"
#include "bytecode.cpp"

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

int
main(int argc, char** argv) {
    
    string source = {};
    if (argc >= 2) {
        source = read_entire_file(argv[2]);
    } else {
        source = read_entire_file("example.c");
    }
    
    // Tokenizer
    Tokenizer tokenizer = {};
    tokenizer.start = source.data;
    tokenizer.end = tokenizer.start + source.count;
    tokenizer.curr = tokenizer.start;
    
    // Parser
    Parser parser = {};
    parser.tokenizer = &tokenizer;
    Ast* ast = parse_block(&parser);
    
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
}

