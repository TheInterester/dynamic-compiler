#include "lex.hpp"

#include <sstream>
#include <iostream>


std::vector<std::tuple<std::string, tokens::TokenType>> 
    special_character_vec {
        
        {",", tokens::TokenType::COMMA},
        {"(", tokens::TokenType::LPAREN},
        {")", tokens::TokenType::RPAREN},
        {"{", tokens::TokenType::LBRACE},
        {"}", tokens::TokenType::RBRACE},
        {"[", tokens::TokenType::LBRACKET},
        {"]", tokens::TokenType::RBRACKET},
        {";", tokens::TokenType::SEMI},
        {":", tokens::TokenType::COLON},
        {".", tokens::TokenType::DOT},

        {"==", tokens::TokenType::EQ},
        {"<=", tokens::TokenType::LE},
        {">=", tokens::TokenType::GE},
        {"<", tokens::TokenType::LT},
        {">", tokens::TokenType::GT},
        {"!", tokens::TokenType::NOT},
        {"&", tokens::TokenType::AND},
        {"|", tokens::TokenType::OR},

        {"+", tokens::TokenType::ADD},
        {"-", tokens::TokenType::SUB},
        {"*", tokens::TokenType::MUL},
        {"/", tokens::TokenType::DIV},

        {"=", tokens::TokenType::ASSIGN},
    };

std::vector<std::tuple<std::string, tokens::TokenType>> 
    keyword_vec {
        {"true", tokens::TokenType::TRUE},
        {"false", tokens::TokenType::FALSE},
        {"fun", tokens::TokenType::FUN},
        {"global", tokens::TokenType::GLOBAL},
        {"if", tokens::TokenType::IF},
        {"else", tokens::TokenType::ELSE},
        {"while", tokens::TokenType::WHILE},
        {"return", tokens::TokenType::RETURN},
        {"None", tokens::TokenType::NONE},


    };

lexcode::Lexer::Lexer(const std::string &file_contents)
    : rest(file_contents), current_line(1), current_col(1) {}

bool lexcode::Lexer::is_eof() { return this->rest.empty(); };

char lexcode::Lexer::peek() const { return this->rest.at(0); };

std::string lexcode::Lexer::consume(int n) {
    for (size_t i = 0; i < (size_t)n; i++) {
        if (i >= this->rest.length())
            break;

        if (this->rest[i] == '\n') {
            this->current_col = 1;
            this->current_line += 1;
        } else {
            this->current_col += 1;
        }
    }

    std::string text = this->rest.substr(0, n);
    this->rest.erase(0, n);
    return text;
}

template <int (*predicate)(int)> std::string lexcode::Lexer::take_while() const {
    size_t i = 0;
    while (i < this->rest.length() && predicate(this->rest[i])) {
        i++;
    }
  return this->rest.substr(0, i);
};

std::vector<lexcode::LexToken> lexcode::Lexer::lex() {
    std::vector<lexcode::LexToken> res;
    while (!is_eof()) {
        
        if (lex_whitespace()) 
            continue;

        if (lex_comment())
            continue;
        
        if (auto token = lex_symbol()) {
            res.push_back(*token);
            continue;
        }

        if (auto token = lex_stringliteral()) {
            res.push_back(*token);
            continue;
        };

        if (auto token = lex_intliteral()) {
            res.push_back(*token);
            continue;
        };

        if (auto token = lex_identifier_or_keyword()) {
            res.push_back(*token);
            continue;
        };

        std::cerr << "Unrecognized character " << peek() << " at " << current_line << std::endl;
        std::exit(1);
    }
    //res.emplace_back(tokens::TokenType::EOFT, "", current_line);
    //os << std::endl;
    return res;
}



static int is_comment(int c) { return c != '\r' && c != '\n'; };

bool lexcode::Lexer::lex_comment() {
    if (this->rest.substr(0, 2) == "//") {
        std::string comment = take_while<is_comment>();
        consume(comment.length());
        return true;
    }
    return false;
};

static int is_id_or_key(int c) { return std::isalnum(c) || c == '_'; };

std::optional<lexcode::LexToken> lexcode::Lexer::lex_identifier_or_keyword() {
    if (std::isalpha(peek()) || peek() == '_') {
        std::string text;
        text.append(consume(1));
        if (!is_eof()) {
            text.append(take_while<is_id_or_key>());
            consume(text.length()-1);
        }
        
        for (const auto &[keyword, token] : keyword_vec) {
            //std::cout << keyword << ", " << text << std::endl;
            if (text == keyword) {
                return lexcode::LexToken(token, text, this->current_line);
            }
        }
        return lexcode::LexToken(tokens::TokenType::IDENTIFIER, text, this->current_line);

    } return std::nullopt;
};


std::optional<lexcode::LexToken> lexcode::Lexer::lex_stringliteral() {
    if (peek() == '"') {
        auto start_line = this->current_line;
        std::ostringstream oss;
        oss << consume(1);
        while (!is_eof() && peek() != '"') {
            if (peek() == '\\') {
                oss << consume(1);
                if (!is_eof()) {
                    int escaped = peek();
                    if ((escaped == 'n' || escaped == 't' || escaped == '\\' || escaped == '"')) {
                        oss << consume(1);
                    } else {
                        std::cerr << "Invalid escape sequence \\" << escaped << " at line " << current_line << std::endl;
                        std::exit(1);
                    }
                }
            } else { 
                oss << consume(1);
            }
        }
        if (is_eof()) {
            std::cerr << "Unterminated string literal starting at " << start_line << std::endl;
            std::exit(1);
        } else oss << consume(1);
        std::string text = oss.str();
        std::string processed_text = process_escapes(text.substr(1, text.length() - 2));

        return lexcode::LexToken(tokens::TokenType::STRINGLITERAL, processed_text, start_line);

    } return std::nullopt;
}

static int is_intliteral(int c) { return std::isdigit(c); };

std::optional<lexcode::LexToken> lexcode::Lexer::lex_intliteral() {
    if (isdigit(peek()) || (peek() == '-' && isdigit(this->rest[1]))) {
        std::string text = consume(1);
        text.append(take_while<is_intliteral>());
        consume(text.length()-1);
        return lexcode::LexToken(tokens::TokenType::INTLITERAL, text, this->current_line);
    } return std::nullopt;
};

std::string lexcode::Lexer::process_escapes(std::string in) {
    std::ostringstream oss;
    size_t i = 0;
    while (i < in.length()) {
        if (in[i] == '\\') {
            if (i+1 >= in.length()) {
                std::cerr << "what the actual fuck bad escape string???" << std::endl;
                std::exit(1);
            }
            switch (in[i+1]) {
                case 'n':
                    oss << '\n';
                    break;
                case 't':
                    oss << '\t';
                    break;
                case '\\':
                    oss << '\\';
                    break;
                case '\"':
                    oss << '\"';
            }
            i += 2;
        } else {
            oss << in[i];
            i++;
        }

    }
    return oss.str();
}

static int is_whitespace(int c) { return std::isspace(c); };

bool lexcode::Lexer::lex_whitespace() {
    std::string hmm = take_while<is_whitespace>();
    consume(hmm.length());
    return hmm.length()>0;
}

std::optional<lexcode::LexToken> lexcode::Lexer::lex_symbol() {
    int line = this->current_line;
    for (const auto &[symbol, type] : special_character_vec) {
        if (this->rest.substr(0, symbol.length()) == symbol) {
            std::string text = consume(symbol.length());
            return lexcode::LexToken(type, text, line);
        }
    } return std::nullopt;
}

std::vector<lexcode::LexToken> lexcode::lex(const std::string contents) {
    auto lexer = lexcode::Lexer(contents);
    return lexer.lex();
};