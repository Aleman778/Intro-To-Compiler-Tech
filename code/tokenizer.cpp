struct Tokenizer {
    u8* start;
    u8* end;
    u8* curr;
};

enum Token_Kind {
    Token_Invalid,
    Token_Whitespace,
    Token_Semi,
    Token_Assign,
    Token_Number,
    Token_Ident,
    Token_EOF,
};

struct Token {
    Token_Kind kind;
    string source;
};

inline bool
is_ident_start(u8 c) {
    return (('a' <= c && c <= 'z') ||
            ('A' <= c && c <= 'Z') ||
            '_' == c || 
            '$' == c);
}

inline bool
is_ident_continue(u8 c) {
    return (('a' <= c && c <= 'z') ||
            ('A' <= c && c <= 'Z') ||
            ('0' <= c && c <= '9') ||
            '_' == c || 
            '$' == c);
}

inline bool
is_whitespace(u8 c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline bool
is_number(u8 c) {
    return c >= '0' && c <= '9';
}

inline void
scan_while(Tokenizer* tokenizer, bool predicate(u8)) {
    while (predicate(*tokenizer->curr) && tokenizer->curr <= tokenizer->end) {
        tokenizer->curr++;
    }
}

Token
advance_token(Tokenizer* tokenizer) {
    Token result = {};
    u8* base = tokenizer->curr;
    
    if (tokenizer->curr >= tokenizer->end) {
        result.kind = Token_EOF;
        return result;
    }
    
    u8 c = *tokenizer->curr;
    if (is_whitespace(c)) {
        scan_while(tokenizer, &is_whitespace);
        result.kind = Token_Whitespace;
    } else if (is_number(c)) {
        scan_while(tokenizer, &is_number);
        result.kind = Token_Number;
    } else if (is_ident_start(c)) {
        tokenizer->curr++;
        scan_while(tokenizer, &is_ident_continue);
        result.kind = Token_Ident;
    } else if (c == '=') {
        tokenizer->curr++;
        result.kind = Token_Assign;
    } else if (c == ';') {
        tokenizer->curr++;
        result.kind = Token_Semi;
    } else {
        result.kind = Token_Invalid;
    }
    
    result.source = string_view(base, tokenizer->curr);
    return result;
}
