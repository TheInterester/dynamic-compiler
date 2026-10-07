#include "printer.hpp"

#include <iostream>
#include <ostream>
#include <sstream>
#include <string>


std::string process_escapes(std::string in) {
    std::ostringstream oss;
    for (const char c : in) {
        switch (c) {
            case '\n':
                oss << "\\n";
                break;
            case '\t':
                oss << "\\t";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\"':
                oss << "\\\"";
                break;  
            default:
                oss << c;
                break;          
        }
    }
    return oss.str();
}

std::string printer::print_token(lexcode::LexToken token) {
    std::ostringstream oss;
    std::string type_text;
        switch (token.type) {
            case tokens::TokenType::INTLITERAL:
                type_text = " INTLITERAL ";
                break;
            case tokens::TokenType::STRINGLITERAL:
                type_text = " STRINGLITERAL ";
                break;
            case tokens::TokenType::TRUE:
                type_text = " BOOLEANLITERAL ";
                break;
            case tokens::TokenType::FALSE:
                type_text = " BOOLEANLITERAL ";
                break;
            case tokens::TokenType::IDENTIFIER:
                type_text = " IDENTIFIER ";
                break;
            default:
                type_text = " ";
                break;
        }
        if (token.type == tokens::TokenType::STRINGLITERAL) {
            oss << token.line << type_text << '"' << process_escapes(token.text) << '"';
        } else {
            oss << token.line << type_text << token.text;
        }
    return oss.str();
}

std::string printer::print_lex(std::vector<lexcode::LexToken> tokens) {
    std::ostringstream oss;
    for (size_t i = 0; i < tokens.size(); i++) {
        oss << print_token(tokens[i]) << std::endl;
    }
    return oss.str();
}

std::string printer::print_ast(ast::ParseNode& root) {
    (void)root;
    return "AST print currently nonfunctional";
    //return root.str();
}