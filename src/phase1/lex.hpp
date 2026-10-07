#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>
#include "tokens.hpp"


namespace lexcode {





struct LexToken {
    tokens::TokenType type;
    std::string text;
    int line;

    LexToken(tokens::TokenType t, std::string s, int l) : type(t), text(std::move(s)), line(l) {}
};

//typedef std::vector<LexToken> TokenArray;

class Lexer {
    public:
        Lexer(const std::string &file_contents);
    
        std::vector<LexToken> lex();
    
    private:
        std::string rest;
        int current_line;
        int current_col;

        char peek() const;
        std::string consume(int n);
        template <int (*predicate)(int)> std::string take_while() const;

        bool is_eof();
        bool lex_whitespace();
        bool lex_comment();
        std::optional<LexToken> lex_symbol();
        std::optional<LexToken> lex_intliteral();
        std::optional<LexToken> lex_identifier_or_keyword();
        std::optional<LexToken> lex_stringliteral();
        std::optional<LexToken> lex_keyword(); //unimpl

        std::string process_escapes(std::string);

};

std::vector<LexToken> lex(const std::string contents);
}