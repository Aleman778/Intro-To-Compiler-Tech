
struct Interp_Scope {
    map(string_id, Value)* locals;
    string_id name;
};

struct Interp {
    array(Interp_Scope)* scopes;
};

void
interp_save_value(Interp* interp, string_id ident, Value value) {
    assert(array_count(interp->scopes) > 0);
    Interp_Scope current_scope = interp->scopes[array_count(interp->scopes) - 1];
    map_put(current_scope.locals, ident, value);
}

Value
interp_load_value(Interp* interp, string_id ident) {
    assert(array_count(interp->scopes) > 0);
    Interp_Scope current_scope = interp->scopes[array_count(interp->scopes) - 1];
    return map_get(current_scope.locals, ident);
}

Value 
interp_expression(Interp* interp, Ast* ast) {
    Value result = {};
    
    switch (ast->kind) {
        case Ast_Value: {
            result = ast->Value;
        } break;
        
        case Ast_Ident: {
            result = interp_load_value(interp, ast->Ident);
        } break;
        
        case Ast_Binary: {
            Value lhs_op = interp_expression(interp, ast->Binary.lhs);
            Value rhs_op = interp_expression(interp, ast->Binary.rhs);
            // ...
            if (ast->Binary.op == Binop_Assign) {
                if (ast->Binary.lhs->kind == Ast_Ident) {
                    string_id ident = ast->Binary.lhs->Ident;
                    interp_save_value(interp, ident, rhs_op);
                } // ...
            }
        } break;
    }
    return result;
}
