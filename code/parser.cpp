
// Parser

struct Parser {
    Tokenizer* tokenizer;
    Token curr_token;
    Token peek_token;
    
    Memory_Arena ast_arena;
};

Token
next_token(Parser* parser) {
    if (parser->peek_token.kind != Token_Invalid) {
        // Consumes peeked token
        parser->curr_token = parser->peek_token;
        parser->peek_token.kind = Token_Invalid;
        return parser->curr_token;
    }
    
    parser->curr_token = advance_token(parser->tokenizer);
    if (parser->curr_token.kind == Token_Whitespace) {
        parser->curr_token = advance_token(parser->tokenizer);
    }
    return parser->curr_token;
};

Token
peek_token(Parser* parser) {
    if (parser->peek_token.kind == Token_Invalid) {
        parser->peek_token = advance_token(parser->tokenizer);
        if (parser->peek_token.kind == Token_Whitespace) {
            parser->peek_token = advance_token(parser->tokenizer);
        }
    }
    return parser->peek_token;
}


// String interner

typedef u32 string_id;

struct String_Interner {
    string_map(string_id)* str_to_id = 0; // stb_ds hashmap maps strings to ids
    string* id_to_str = 0; // stb_ds array of strings where the index is the string id
    u32 id_counter = 1;
};

global String_Interner global_interner;

string_id
vars_save_cstring(cstring s) {
    string_id id = string_map_get(global_interner.str_to_id, s);
    if (!id) {
        id = global_interner.id_counter++;
        string_map_put(global_interner.str_to_id, s, id);
        array_push(global_interner.id_to_str, string_lit(s));
    }
    return id;
}

string_id vars_save_string(string s) {
    cstring cs = string_to_cstring(s);
    return vars_save_cstring(cs);
}

string vars_load_string(string_id id) {
    string result = {};
    if (id < array_count(global_interner.id_to_str)) {
        result = global_interner.id_to_str[id];
    }
    return result;
}

// Value

enum Value_Type {
    Value_void,
    Value_integer,
    Value_floating,
};

struct Value {
    Value_Type type;
    union {
        int integer;
        float floating;
    };
};


// Abstract Syntax Tree (AST)

enum Binary_Op {
    Binop_Assign,
    Binop_Add,
    Binop_Sub,
    Binop_Mul,
    Binop_Div,
};

enum Ast_Kind {
    Ast_None,
    Ast_Value,
    Ast_Ident,
    Ast_Binary,
    Ast_Block,
};

struct Ast {
    Ast_Kind kind;
    union {
        Value Value;
        string_id Ident;
        struct {
            Ast* lhs;
            Binary_Op op;
            Ast* rhs;
        } Binary;
        struct {
            array(Ast*)* exprs;
        } Block;
    };
};


// Parser implementation

Ast*
parse_identifier(Parser* parser, Token token) {
    Ast* result = arena_push_struct(&parser->ast_arena, Ast);
    
    if (token.kind == Token_Ident) {
        result->kind = Ast_Ident;
        result->Ident = vars_save_string(token.source);
    } else {
        printf("error: parser expected identifier");
    }
    
    return result;
}

Ast*
parse_number(Parser* parser, Token token) {
    assert(token.kind == Token_Number);
    
    const u64 base = 10;
    u64 value = 0;
    int curr_index = 0;
    
    string source = token.source;
    for (int i = 0; i < source.count; i++) {
        u8 c = source.data[i];
        if (c == '_') continue;
        
        s32 d = c - '0';
        assert(d >= 0 && d <= 9 && "tokenization bug");
        
        u64 x = value * base;
        if (value != 0 && x / base != value) {
            printf("error: integer is too large");
            break;
        }
        
        u64 y = x + d;
        if (y < x) { // NOTE(alexander): this should work since d is small compared to x
            printf("error: integer is too large");
            value = 0;
            break;
        }
        
        value = y;
    }
    
    Ast* result = arena_push_struct(&parser->ast_arena, Ast);
    result->kind = Ast_Value;
    result->Value.type = Value_integer;
    result->Value.integer = value;
    return result;
    
}


Ast*
create_binary_expr(Parser* parser, Ast* lhs, Binary_Op op, Ast* rhs) {
    Ast* result = arena_push_struct(&parser->ast_arena, Ast);
    result->kind = Ast_Binary;
    result->Binary.lhs = lhs;
    result->Binary.op = op;
    result->Binary.rhs = rhs;
    return result;
}

Ast*
parse_atom(Parser* parser) {
    Token token = next_token(parser);
    switch (token.kind) {
        case Token_Ident: {
            return parse_identifier(parser, token);
        } break;
        
        case Token_Number: {
            return parse_number(parser, token);
        } break;
    }
    
    return 0;
}

Ast*
parse_expression(Parser* parser) {
    Ast* lhs = parse_atom(parser);
    
    Token token = peek_token(parser);
    switch (token.kind) {
#define BINARY_CASE(binop) \
case Token_##binop: { \
next_token(parser); \
Ast* rhs = parse_expression(parser); \
return create_binary_expr(parser, lhs, Binop_##binop, rhs); \
} break
        
        BINARY_CASE(Assign);
        BINARY_CASE(Add);
        BINARY_CASE(Sub);
        BINARY_CASE(Mul);
        BINARY_CASE(Div);
#undef BINARY_CASE
    }
    
    return lhs;
}


Ast*
parse_block(Parser* parser) {
    Ast* result = arena_push_struct(&parser->ast_arena, Ast);
    result->kind = Ast_Block;
    
    for (;;) {
        Token peek = peek_token(parser);
        if (peek.kind == Token_EOF) {
            break;
        }
        
        Ast* expr = parse_expression(parser);
        array_push(result->Block.exprs, expr);
        
        Token token = next_token(parser);
        if (token.kind != Token_Semi) {
            if (next_token(parser).kind != Token_EOF) {
                pln("error: parser expected semicolon");
            }
            break;
        }
    }
    
    return result;
}