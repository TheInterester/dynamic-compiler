#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#define upp std::unique_ptr<ast::ParseNode>
#define mv(A) std::move(A)
namespace ast {
    class ParseNode;
    class Block;
    class Closure;
    class Global;
    class Assignment;
    class If;
    class While;
    class Return;
    class FunctionDecl;
    class BinOp;
    class UnOp;
    class FieldDereference;
    class IndexExpression;
    class Call;
    class Record;
    class Identifier;
    class Integer;
    class String;
    class None;
    class Boolean;
    class Nop;
    
    class Visitor {
        public:
            virtual void visit(Identifier &identifier) = 0;
            virtual void visit(Block &block) = 0;
            virtual void visit(Closure &closure) = 0;
            virtual void visit(Global &global) = 0;
            virtual void visit(If &if_) = 0;
            virtual void visit(While &while_) = 0;
            virtual void visit(Return &return_) = 0;
            virtual void visit(Assignment &assignment) = 0;
            virtual void visit(Call &call) = 0;
            virtual void visit(FunctionDecl &function_decl) = 0;
            virtual void visit(Record &record) = 0;
            virtual void visit(BinOp &binop) = 0;
            virtual void visit(FieldDereference &field_deref) = 0;
            virtual void visit(IndexExpression &idx_expr) = 0;
            virtual void visit(UnOp &unop) = 0;
            virtual void visit(Integer &integer) = 0;
            virtual void visit(String &string) = 0;
            virtual void visit(None &none) = 0;
            virtual void visit(Boolean &boolean) = 0;
            virtual void visit(Nop &nop) = 0;
    };

    enum class BinaryOp {ADD, SUB, MUL, DIV, LE, LT, GE, GT, EQ, OR, AND};

    enum class UnaryOp {NOT, NEG};

    enum class LocationOp {FIELD, INDEX, NOP};

    class ParseNode {
        public:
            virtual void accept(Visitor &visitor) = 0;
            bool intFoldable = false;
            bool stringFoldable = false;
            bool boolFoldable = false;
            void setIntFoldable(bool value) {
                this->intFoldable = value;
            }
            void setStringFoldable(bool value) {
                this->intFoldable = value;
            }
            void setBoolFoldable(bool value) {
                this->intFoldable = value;
            }

    };

    class Identifier : public ParseNode {
        public:
        std::string name;
        Identifier(const std::string &name) : name(name) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Global : public ParseNode {
        public:
        std::string name;

        Global(const std::string &name) : name(name) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class If : public ParseNode {
        public:
        upp condition;
        upp then;
        upp else_;

        If(upp condition, upp then, upp else_) 
        : condition(mv(condition)), then(mv(then)), else_(mv(else_)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class While : public ParseNode {
        public:
        upp condition;
        upp body;

        While(upp condition, upp body)
        : condition(mv(condition)), body(mv(body)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Return : public ParseNode {
        public:
        upp expr;

        Return(upp expr) : expr(mv(expr)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Assignment : public ParseNode {
        public:
        upp target;
        upp value;

        Assignment(upp target, upp value) 
        : target(mv(target)), value(mv(value)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Call : public ParseNode {
        public:
        const upp location;
        std::vector<upp> args;

        Call(upp location, std::vector<upp> && args)
        : location(mv(location)), args(mv(args)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class FunctionDecl : public ParseNode {
        public:
        std::vector<std::unique_ptr<ast::Identifier>> args;
        std::unique_ptr<ast::Closure> body;

        FunctionDecl(std::vector<std::unique_ptr<ast::Identifier>> &&args, std::unique_ptr<ast::Closure> body) 
        : args(std::move(args)), body(mv(body)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Record : public ParseNode {
        public:
        std::vector<std::pair<std::string, upp>> fields;

        Record(std::vector<std::pair<std::string, upp>> && fields) : fields(mv(fields)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class BinOp : public ParseNode {
        public:
            BinaryOp op;
            upp lhs;
            upp rhs;
            
            BinOp(BinaryOp op, upp lhs, upp rhs) 
            : op(op), lhs(mv(lhs)), rhs(mv(rhs)) {
                using BO = BinaryOp;
                switch(op) {
                    case BO::ADD: setIntFoldable(this->lhs->intFoldable && this->rhs->intFoldable); 
                                  setStringFoldable(this->lhs->stringFoldable || this->rhs->stringFoldable); break; 
                    case BO::SUB: setIntFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::MUL: setIntFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::DIV: setIntFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::AND: setBoolFoldable(this->lhs->boolFoldable && this->rhs->boolFoldable); break;
                    case BO::OR:  setBoolFoldable(this->lhs->boolFoldable && this->rhs->boolFoldable); break;
                    case BO::GT:  setBoolFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::LT:  setBoolFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::LE:  setBoolFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::GE:  setBoolFoldable(this->lhs->intFoldable && this->rhs->intFoldable); break;
                    case BO::EQ: break; //ignore for now
                };
            }
            void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class FieldDereference : public ParseNode {
        public:
        upp base;
        std::unique_ptr<ast::Identifier> field;

        FieldDereference(upp base, std::unique_ptr<ast::Identifier> field)
        : base(mv(base)), field(mv(field)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class IndexExpression : public ParseNode {
        public:
        upp base;
        upp index;
        IndexExpression(upp base, upp index)
        : base(mv(base)), index(mv(index)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class UnOp : public ParseNode {
        public:
        UnaryOp op;
        upp expr;

        UnOp(UnaryOp op, upp expr) : op(op), expr(mv(expr)) {
            setIntFoldable(op == UnaryOp::NEG && this->expr->intFoldable);
            setBoolFoldable(op == UnaryOp::NOT && this->expr->boolFoldable);
        }
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Integer : public ParseNode {
        public:
        int value;

        Integer(int value) : value(value) { setIntFoldable(true); }
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class String : public ParseNode {
        public:
        std::string value;
        //bool foldable = true;

        String(const std::string &value) : value(value) { setStringFoldable(true); }
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class None : public ParseNode {
        public:
        None() {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Boolean : public ParseNode {
        public:
        bool value;
        //bool foldable = true;
        
        Boolean(bool value) : value(value) { setBoolFoldable(true); }
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Closure : public ParseNode {
        public:
        upp body;
        Closure(upp body) : body(mv(body)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Nop : public ParseNode {
        public:
        Nop() {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };

    class Block : public ParseNode {
        public:
        upp left;
        upp right;
        Block(upp left, upp right) : left(mv(left)), right(mv(right)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); };
    };
}