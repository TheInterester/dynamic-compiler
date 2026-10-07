#include "../printer.hpp"
#include "parse.hpp"
#include <iostream>
#include <ostream>
#include <sstream>

#define upp std::unique_ptr<ast::ParseNode>
#define mv(A) std::move(A)

typedef tokens::TokenType TT;
typedef tokens::NonTerminalType NTT;

std::unique_ptr<ast::ParseNode> parse(const std::string file_contents) {
    auto tokens = lexcode::lex(file_contents);
    auto parser = parsecode::Parser(tokens);

    return parser.parse();
}

//ParseNode section
std::unique_ptr<ast::ParseNode> parsecode::Parser::parse() {
    auto root = parse_block();
    if (!is_eof()) {
        std::cerr << "Unexpected token: " << peek().text << " on line " << peek().line << std::endl;
        std::exit(1);
    }
    return root;
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_block() {
    auto first = parse_line();
    while (!is_eof() && !check({TT::RBRACE})) {
        auto second = parse_line();
        first = std::make_unique<ast::Block>(std::move(first), std::move(second));
    }
    return first;
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_line() {
    if (match({TT::GLOBAL})) {
        return parse_global();
    } else if (match({TT::IF})) {
        return parse_if();
    } else if (match({TT::WHILE})) {
        return parse_while();
    } else if (match({TT::RETURN})) {
        return parse_return();
    } else if (check(TT::IDENTIFIER)) {
        auto left = parse_location();
        if (match({TT::ASSIGN})) {
            auto right = parse_expr();
            consume(TT::SEMI, "Expected `;`");
            return std::make_unique<ast::Assignment>(std::move(left), std::move(right));
        } else if (match({TT::LPAREN})) {
            std::vector<upp> args;
            while (!match({TT::RPAREN})) {
                auto arg = parse_expr();
                args.emplace_back(mv(arg));
                if (!match({TT::COMMA})) {
                    consume(TT::RPAREN, "Expected `)`");
                    break;
                }
            }
            consume(TT::SEMI, "Expected `;`");
            return std::make_unique<ast::Call>(std::move(left), std::move(args));
        }
    } else if (is_eof() || check({TT::RBRACE})) {
        return std::make_unique<ast::Nop>();
    }
    std::cerr << "Unexpected token " << peek().text << "@ line " << peek().line << std::endl;
    std::exit(1);

}

std::unique_ptr<ast::Identifier> parsecode::Parser::parse_identifier() {
    consume(TT::IDENTIFIER, "Expected identifier");
    return std::make_unique<ast::Identifier>(previous().text);
}

std::unique_ptr<ast::Global> parsecode::Parser::parse_global() {
    auto token = consume(TT::IDENTIFIER, "Expected identifier");
    auto semi = consume(TT::SEMI, "Expected ;");
    return std::make_unique<ast::Global>(token.text);
}

std::unique_ptr<ast::If> parsecode::Parser::parse_if() {
    consume(TT::LPAREN, "Expexcted `(` after if");
    auto condition = parse_expr();
    consume(TT::RPAREN, "Expected `)`");
    consume(TT::LBRACE, "Expected `{`");
    auto then = parse_block();
    consume(TT::RBRACE, "Expected `}`");
    auto else_ = std::unique_ptr<ast::ParseNode>();
    if (match({TT::ELSE})) {
        consume(TT::LBRACE, "Expected `{`");
        else_ = parse_block();
        consume(TT::RBRACE, "Expected `}`");
    } else {
        else_ = std::make_unique<ast::Nop>();
    }
    return std::make_unique<ast::If>(std::move(condition), std::move(then), std::move(else_));
}

std::unique_ptr<ast::While> parsecode::Parser::parse_while() {
    consume(TT::LPAREN, "Expected `(`");
    auto condition = parse_expr();
    consume(TT::RPAREN, "Expected `)");
    consume(TT::LBRACE, "Expected `{`");
    auto body = parse_block();
    consume(TT::RBRACE, "Expected `}`");
    return std::make_unique<ast::While>(std::move(condition), std::move(body));
};

std::unique_ptr<ast::Return> parsecode::Parser::parse_return() {
    auto expr = parse_expr();
    consume(TT::SEMI, "Expected `;`");
    return std::make_unique<ast::Return>(std::move(expr));
};

//parse_assignment
//parse_call

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_expr() {
    if (match({TT::FUN})) {
        return parse_function_decl();
    } else if (match({TT::LBRACE})) {
        return parse_record();
    }
    return parse_s_expr();
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_s_expr() {
    return parse_cond_or();
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_cond_or() {
    auto left = parse_cond_and();
    while (match({TT::OR})) {
        auto op = ast::BinaryOp::OR;
        auto right = parse_cond_and();
        left = std::make_unique<ast::BinOp>(op, mv(left), mv(right));
    }
    return left;
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_cond_and() {
    auto left = parse_cond_not();
    while (match({TT::AND})) {
        auto op = ast::BinaryOp::AND;
        auto right = parse_cond_not();
        left = std::make_unique<ast::BinOp>(op, mv(left), mv(right));
    }
    return left;
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_cond_not() {
    if (match({TT::NOT})) {
        auto op = ast::UnaryOp::NOT;
        auto expr = parse_cond_not(); //sus
        return std::make_unique<ast::UnOp>(op, mv(expr));
    }
    return parse_rel();
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_rel() {
    auto left = parse_add();
    while (check({TT::LE, TT::LT, TT::GE, TT::GT, TT::EQ})) {
        ast::BinaryOp op;
        switch (advance().type) {
            case TT::LE:
                op = ast::BinaryOp::LE;
                break;
            case TT::LT:
                op = ast::BinaryOp::LT;
                break;
            case TT::GE:
                op = ast::BinaryOp::GE;
                break;
            case TT::GT:
                op = ast::BinaryOp::GT;
                break;
            case TT::EQ:
                op = ast::BinaryOp::EQ;
                break;
            default:
                std::cerr << "Unreachable state" << std::endl;
                std::exit(-1);
        }
        auto right = parse_add();
        left = std::make_unique<ast::BinOp>(op, mv(left), mv(right));
    }
    return left;
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_add() {
    auto left = parse_mul();
    while (check({TT::ADD, TT::SUB})) {
        auto op = (check(TT::ADD)) ? ast::BinaryOp::ADD : ast::BinaryOp::SUB;
        advance();
        auto right = parse_mul();
        left = std::make_unique<ast::BinOp>(op, mv(left), mv(right));
    }
    return left;
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_mul() {
    auto left = parse_atom();
    while (check({TT::MUL, TT::DIV})) {
        auto op = (check(TT::MUL)) ? ast::BinaryOp::MUL : ast::BinaryOp::DIV;
        advance();
        auto right = parse_atom();
        left = std::make_unique<ast::BinOp>(op, mv(left), mv(right));
    }
    return left;
}

std::unique_ptr<ast::FunctionDecl> parsecode::Parser::parse_function_decl() {
    consume(TT::LPAREN, "Expected `(`");
    std::vector<std::unique_ptr<ast::Identifier>> args;
    while (!match({TT::RPAREN})) {
        auto id = parse_identifier();
        args.emplace_back(mv(id));
        if (!match({TT::COMMA})) {
            consume(TT::RPAREN, "Expected `)`");
            break;
        }
    }
    consume(TT::LBRACE, "Expected `{`");
    auto body = parse_closure();
    return std::make_unique<ast::FunctionDecl>(std::move(args), mv(body));
}

std::unique_ptr<ast::Record> parsecode::Parser::parse_record() {

    std::vector<std::pair<std::string, upp>> fields;
    while (!match({TT::RBRACE})) {
        if (is_eof()) {
            std::cerr << "Unexpected end of field" << std::endl;
            std::exit(1);
        }
        if (!check(TT::IDENTIFIER)) {
           consume(TT::IDENTIFIER, "Expected identifier");
        }
        auto name = parse_identifier();
        auto name_str = previous().text;
        consume(TT::COLON, "Expected `:`");
        auto value = parse_expr();
        consume(TT::SEMI, "Expected `;`");
        fields.emplace_back(std::make_pair(name_str, mv(value)));
    }
    return std::make_unique<ast::Record>(mv(fields));
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_location() {
    std::unique_ptr<ast::ParseNode> base = parse_identifier();
    while (check({TT::DOT, TT::LBRACKET})) {
        if (match({TT::DOT})) {
            auto field = parse_identifier();
            base = std::make_unique<ast::FieldDereference>(mv(base), mv(field));
        } else if (match({TT::LBRACKET})) {
            auto index = parse_expr();
            consume(TT::RBRACKET, "Expected `]`");
            base = std::make_unique<ast::IndexExpression>(mv(base), mv(index));
        }
    }
    return base;
}



std::unique_ptr<ast::Closure> parsecode::Parser::parse_closure() {
    auto body = parse_block();
    consume(TT::RBRACE, "Expected `}`");
    return std::make_unique<ast::Closure>(std::move(body));
}

std::unique_ptr<ast::ParseNode> parsecode::Parser::parse_atom() {
    if (match({TT::LPAREN})) {
        auto expr = parse_s_expr();
        consume(TT::RPAREN, "Expected `)`");
        return expr; 
    } else if (match({TT::SUB})) {
        auto expr = parse_atom();
        return std::make_unique<ast::UnOp>(ast::UnaryOp::NEG, std::move(expr));
    } else if (match({TT::NONE})) {
        return std::make_unique<ast::None>();
    } else if (match({TT::TRUE})) {
        return std::make_unique<ast::Boolean>(true);
    } else if (match({TT::FALSE})) {
        return std::make_unique<ast::Boolean>(false);
    } else if (match({TT::STRINGLITERAL})) {
        return std::make_unique<ast::String>(previous().text);
    } else if (match({TT::INTLITERAL})) {
        return std::make_unique<ast::Integer>(std::stoi(previous().text));
    } else if (check(TT::IDENTIFIER)) {
        auto location = parse_location();
        if (match({TT::LPAREN})) {
            std::vector<upp> args;
            while(!match({TT::RPAREN})) {
                auto arg = parse_expr();
                args.emplace_back(mv(arg));
                if (!match({TT::COMMA})) {
                    consume(TT::RPAREN, "Expected `)`");
                    break;
                }
            }
            return std::make_unique<ast::Call>(mv(location), mv(args));
        } else {
            return location;
        }
    }
    std::cerr << "Unexpected token " << peek().text << std::endl;
    std::exit(1);
}

bool parsecode::Parser::is_eof() const { return idx >= tokens.size(); }

bool parsecode::Parser::check(tokens::TokenType type) const {
    return !is_eof() && tokens[idx].type == type;
}

bool parsecode::Parser::check(std::initializer_list<tokens::TokenType> types) const {
    for (auto &type : types) {
        if (check(type)) return true;
    }
    return false;
}

const lexcode::LexToken &parsecode::Parser::peek() const { return tokens[idx]; }

const lexcode::LexToken &parsecode::Parser::previous() const { return tokens[idx-1]; }

//Move forward one token and return it
lexcode::LexToken parsecode::Parser::advance() { 
    if (!is_eof()) idx++;
    return previous();
};

//Return true if next token matches any of the provided types
bool parsecode::Parser::match(std::initializer_list<tokens::TokenType> types) {
    if (check(types)) {
        advance();
        return true;
    }
    return false;
}

//Consumed next token 
lexcode::LexToken parsecode::Parser::consume(tokens::TokenType type, const std::string &message) {
    if (check(type)) {
        return advance();
    }
    const lexcode::LexToken &token = peek();
    std::cerr << message << ", got " << token.text << " on line " << token.line << std::endl;
    std::exit(1);
}

std::unique_ptr<ast::ParseNode> parsecode::parse(const std::string &file_contents) {
    auto tokens = lexcode::lex(file_contents);
    auto parser = parsecode::Parser(tokens);
    return parser.parse();
}