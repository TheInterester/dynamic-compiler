#pragma once

#include <vector>
#include <variant>
#include <iostream>
#include <memory>

#include "ast.hpp"
#include "lex.hpp"
#include "tokens.hpp"


namespace parsecode {
    class Parser {
        public:
            Parser(const std::vector<lexcode::LexToken>& tokens) : tokens(tokens), idx(0) {}
            std::unique_ptr<ast::ParseNode> parse();

        private:
            std::vector<lexcode::LexToken> tokens;
            size_t idx;

            bool is_eof() const;
            bool check(tokens::TokenType type) const;
            bool check(std::initializer_list<tokens::TokenType> types) const;
            const lexcode::LexToken &peek() const;
            const lexcode::LexToken &previous() const;
            lexcode::LexToken advance();
            bool match(std::initializer_list<tokens::TokenType> types);
            lexcode::LexToken consume(tokens::TokenType type, const std::string &message);

            std::unique_ptr<ast::ParseNode>     parse_block();
            std::unique_ptr<ast::ParseNode>     parse_line();
            std::unique_ptr<ast::Identifier>    parse_identifier();
            std::unique_ptr<ast::Global>        parse_global();
            std::unique_ptr<ast::If>            parse_if();
            std::unique_ptr<ast::While>         parse_while();
            std::unique_ptr<ast::Return>        parse_return();
            //std::unique_ptr<ast::Assignment>    parse_assignment();
            //std::unique_ptr<ast::Call>          parse_call();
            std::unique_ptr<ast::ParseNode>     parse_expr();
            std::unique_ptr<ast::ParseNode>     parse_s_expr();
            std::unique_ptr<ast::ParseNode>     parse_cond_or();
            std::unique_ptr<ast::ParseNode>     parse_cond_and();
            std::unique_ptr<ast::ParseNode>     parse_cond_not();
            std::unique_ptr<ast::ParseNode>     parse_rel();
            std::unique_ptr<ast::ParseNode>     parse_add();
            std::unique_ptr<ast::ParseNode>     parse_mul();
            //std::unique_ptr<ast::ParseNode>     parse_neg();
            std::unique_ptr<ast::FunctionDecl>  parse_function_decl();
            std::unique_ptr<ast::Record>        parse_record();
            std::unique_ptr<ast::ParseNode>     parse_location();
            std::unique_ptr<ast::Closure>       parse_closure();
            std::unique_ptr<ast::ParseNode>     parse_atom();
    };

    std::unique_ptr<ast::ParseNode> parse(const std::string &file_contents);
}