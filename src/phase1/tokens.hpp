#pragma once

#include <vector>
#include <string>

namespace tokens {
    enum class NonTerminalType {
        //The Normal Ones
        NULLTYPE, //N/A
        PROGRAM,  //Y
        STATEMENT, //Y
        ASSIGNMENT, //P
        EXPR, //P
        SIMPLEEXPR, //Y(?)
        LOCATION, //Y
        BLOCK, //Y
        BINOP, //Y
        ARITHOP, //Y
        COMPOP, //Y
        CONDOP, //Y
        CALL, //Y
        //Non-explicit Non-Hacked
        ARGS, //REWORK
        CARGS, //Y
        ELSE,
        OBJFIELD, //Y

        //The Grammar Hacking Ones (lead w/"P" for clarity)
        PSTATEMENT, //P Remove(?)
        PLOCATION, //Y
        PARGS, //REWORK
        PCARGS, //N
        PELSE, //Y
        PSIMEXPR, //Y
        POBJFIELD,
    };

    enum class TokenType {
        //broad types
        IDENTIFIER,
        INTLITERAL,
        STRINGLITERAL,
        BOOLEANLITERAL,

        //special symbols
            //generic
            COMMA,
            LPAREN,
            RPAREN,
            LBRACE,
            RBRACE,
            LBRACKET,
            RBRACKET,
            SEMI,
            COLON,
            DOT,
            EOFT,

            //logical
            EQ,
            NOT,
            LT,
            LE,
            GT,
            GE,
            AND,
            OR,

            //arith
            ADD,
            SUB,
            MUL,
            DIV,

            //wonky
            ASSIGN,

        //keywords
        TRUE,
        FALSE,
        FUN,
        GLOBAL,
        IF,
        ELSE,
        WHILE,
        RETURN,
        NONE,

        //special null type for parsenodes
        NULLTYPE,
    };

    

}