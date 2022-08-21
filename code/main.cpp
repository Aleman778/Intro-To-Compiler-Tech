#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include "basic.h"

#include "tokenizer.cpp"
#include "parser.cpp"
#include "interp.cpp"
#include "bytecode.cpp"


int
main(int argc, char** argv) {
    cstring source = "x = 10;";
    
    // Tokenizer
    Tokenizer tokenizer = {};
    tokenizer.start = (u8*) source;
    tokenizer.end = (u8*) source + cstring_count(source);
    tokenizer.curr = tokenizer.start;
    
    // Parser
    Parser parser = {};
    parser.tokenizer = &tokenizer;
    Ast* ast = parse_expression(&parser);
    
    // Interpreter
    Interp interp = {};
    Interp_Scope scope = {};
    array_push(interp.scopes, scope);
    Value interp_result = interp_expression(&interp, ast);
    pln("%\n", f_int(interp_result.integer));
    
    // Bytecode builder
    Bc_Builder bc_builder = {};
    bc_build_expression(&bc_builder, ast);
    bc_print_program(&bc_builder);
}

