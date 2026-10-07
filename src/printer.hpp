#pragma once

#include "phase1/ast.hpp"
#include "phase1/lex.hpp"
#include "phase1/parse.hpp"

namespace printer {
    std::string print_lex(std::vector<lexcode::LexToken>);
    std::string print_token(lexcode::LexToken);
    std::string print_ast(ast::ParseNode&);
}