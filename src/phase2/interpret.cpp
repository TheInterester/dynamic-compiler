#pragma once

#include <algorithm>
#include <cstdlib>
#include <unordered_set>
#include <sstream>
#include <functional>
#include <deque>
#include <unordered_map>

#include "../gc/gc.hpp"
#include "interpret.hpp"
#include "../printer.hpp"

class Frame;

class Value;
class Boolean;
class Integer;
class String;
class Record;
class Function;
class None;

class Closure;
class Builtin;
class Return;

class Value : public Collectable {
    public:
        virtual bool *as_boolean() = 0;
        virtual int *as_integer() = 0;
        virtual std::string *as_string() = 0;
        virtual Record *as_record() = 0;
        virtual Function *as_function() = 0;
        virtual None *as_none() = 0;
        virtual Builtin *as_builtin() = 0;
        virtual Return *as_return() = 0;
        virtual std::string to_string() = 0;
        virtual size_t getSize() = 0;

    protected:
        virtual void follow(CollectedHeap& heap) = 0;

};

class Boolean : public Value {
    public:
        bool value;
        Boolean(bool value) : value(value) {}
        bool *as_boolean() override { return &value; };
        int *as_integer() override { return nullptr; };
        std::string *as_string() override { return nullptr; };
        Record *as_record() override { return nullptr; };
        Function *as_function() override { return nullptr; };
        None *as_none() override { return nullptr; };
        Builtin *as_builtin() override { return nullptr; };
        Return *as_return() override { return nullptr; };
        std::string to_string() override { return (value) ? "true" : "false"; };
        size_t getSize() override {
            return sizeof(*this);
        }

    protected:
        void follow(CollectedHeap& heap) override {
        }
};

class Integer : public Value {
        public:
        int value;
        Integer(int value) : value(value) {}
        bool *as_boolean() override { return nullptr; };
        int *as_integer() override { return &value; };
        std::string *as_string() override { return nullptr; };
        Record *as_record() override { return nullptr; };
        Function *as_function() override { return nullptr; };
        None *as_none() override { return nullptr; };
        Builtin *as_builtin() override { return nullptr; };
        Return *as_return() override { return nullptr; };
        std::string to_string() override { return std::to_string(value); };
        size_t getSize() override {
            return sizeof(*this);
        }

    protected:
        void follow(CollectedHeap& heap) override {
        }
};

class String : public Value {
        public:
        std::string value;
        String(std::string value) : value(value) {}
        bool *as_boolean() override { return nullptr; };
        int *as_integer() override { return nullptr; };
        std::string *as_string() override { return &value; };
        Record *as_record() override { return nullptr; };
        Function *as_function() override { return nullptr; };
        None *as_none() override { return nullptr; };
        Builtin *as_builtin() override { return nullptr; };
        Return *as_return() override { return nullptr; };
        std::string to_string() override { return value; };
        size_t getSize() override {
            return sizeof(*this) + value.capacity();
        }

    protected:
        void follow(CollectedHeap& heap) override {
        }
};

class Record : public Value {
    public:
        std::unordered_map<std::string, Value*> fields;
        Record(std::unordered_map<std::string, Value*> fields) : fields(fields) {}
        bool *as_boolean() override { return nullptr; };
        int *as_integer() override { return nullptr; };
        std::string *as_string() override { return nullptr; };
        Record *as_record() override { return this; };
        Function *as_function() override { return nullptr; };
        None *as_none() override { return nullptr; };
        Builtin *as_builtin() override { return nullptr; };
        Return *as_return() override { return nullptr; };
        std::string to_string() override { 
            std::ostringstream oss;
            std::vector<std::string> keys;

            keys.reserve(fields.size());
            for (auto& [key, _value] : fields) {
                keys.push_back(key);
            }
            std::sort(keys.begin(), keys.end());
            oss << "{";
            for (const auto &key : keys) {
                oss << key << ":" << fields[key]->to_string() << " ";
            }
            oss << "}";
            return oss.str();
        };

        size_t getSize() override {
            return sizeof(*this) + fields.size() * 64;
        }
    protected:
        void follow(CollectedHeap& heap) override {
            for (auto& [key, value] : fields) {
                heap.markSuccessors(value);
            }
        }
};

class Frame : public Collectable {
    public:
        Frame* f_parent;
        Frame* f_global;
        std::unordered_set<std::string> globals;
        std::unordered_map<std::string, Value*> locals;

        Frame(Frame* parent, Frame* f_global) 
        : f_parent(parent), f_global(f_global), globals(), locals() {}

        Value *lookup_read(std::string name) {
            if (globals.find(name) != globals.end()) {
                return f_global->lookup_read(name);
            }
            if (locals.find(name) != locals.end()) {
                return locals[name];
            }
            if (f_parent != nullptr) {
                return f_parent->lookup_read(name);
            }
            std::cout << "UninitializedVariableException" << std::endl;
            std::exit(1);
        }

        Frame *lookup_write(std::string name) {
            if (globals.find(name) != globals.end()) {
                return f_global;
            }
            return this;
        }

        size_t getSize() override {
            return sizeof(*this) + locals.size() * 64 + globals.size() * 32;
        }

    protected:
        void follow(CollectedHeap& heap) override {
            for (auto& [name, value] : locals) {
                if (value) {
                    heap.markSuccessors(value);
                }
            }
        }
};

class Function : public Value {
    public:
    Frame* frame;
    ast::Closure* body;
    std::vector<std::string> args;
    Function(Frame* frame, ast::Closure* body, std::vector<std::string> args) 
    : frame(frame), body(body), args(args) {}
    bool *as_boolean() override { return nullptr; };
    int *as_integer() override { return nullptr; };
    std::string *as_string() override { return nullptr; };
    Record *as_record() override { return nullptr; };
    Function *as_function() override { return this; };
    None *as_none() override { return nullptr; };
    Builtin *as_builtin() override { return nullptr; };
    Return *as_return() override { return nullptr; };
    std::string to_string() override { return std::string("FUNCTION"); };
    size_t getSize() override {
        return sizeof(*this) + args.capacity() * sizeof(std::string);
    }
    protected:
        void follow(CollectedHeap& heap) override {
            if (frame) {
                heap.markSuccessors(frame);
            }
        }
};

class None : public Value {
    public:
    None() {}
    bool *as_boolean() override { return nullptr; };
    int *as_integer() override { return nullptr; };
    std::string *as_string() override { return nullptr; };
    Record *as_record() override { return nullptr; };
    Function *as_function() override { return nullptr; };
    None *as_none() override { return this; };
    Builtin *as_builtin() override { return nullptr; };
    Return *as_return() override { return nullptr; };
    std::string to_string() override { return std::string("None"); };
    size_t getSize() override {
        return sizeof(*this);
    }
    protected:
        void follow(CollectedHeap& heap) override {
        }
};

class Builtin : public Value {
    public:
    std::string name;
    Builtin(std::string name) : name(name) {}
    bool *as_boolean() override { return nullptr; };
    int *as_integer() override { return nullptr; };
    std::string *as_string() override { return nullptr; };
    Record *as_record() override { return nullptr; };
    Function *as_function() override { return nullptr; };
    None *as_none() override { return nullptr; };
    Builtin *as_builtin() override { return this; };
    Return *as_return() override { return nullptr; };
    std::string to_string() override { return std::string("FUNCTION"); };
    size_t getSize() override {
        return sizeof(*this) + name.capacity();
    }
    protected:
        void follow(CollectedHeap& heap) override {
        }
};

class Return : public Value {
    public:
    Value* value;
    Return(Value* value) : value(value) {}
    bool *as_boolean() override { return value->as_boolean(); };
    int *as_integer() override { return value->as_integer(); };
    std::string *as_string() override { return nullptr; };
    Record *as_record() override { return value->as_record(); };
    Function *as_function() override { return value->as_function(); };
    None *as_none() override { return value->as_none(); };
    Builtin *as_builtin() override { return value->as_builtin(); };
    Return *as_return() override { return this; };
    std::string to_string() override { return *(value->as_string()); };
    size_t getSize() override {
        return sizeof(*this);
    }
    protected:
        void follow(CollectedHeap& heap) override {
            heap.markSuccessors(value);
        }
};



class Interpreter : public ast::Visitor {
    public:
        Value* rVal_;
        std::deque<Frame*> stack;
        CollectedHeap heap;
        Value* none;
        Interpreter(size_t memLimit) : rVal_(nullptr), stack(), heap(memLimit) {
            none = heap.allocate<None>();

            heap.setRoots([this]() {
                    std::vector<Collectable*> roots;

                    if (rVal_) {
                    roots.push_back(rVal_);
                    }

                    if (none) {
                    roots.push_back(none);
                    }

                    for (auto* frame : stack) {
                    if (frame) {
                    roots.push_back(frame);
                    }
                    }

                    return roots;
                    });

            stack.push_front(heap.allocate<Frame>(nullptr, nullptr));
            stack.back()->f_global = stack.back();
            stack.back()->locals["print"] = heap.allocate<Builtin>("print");
            stack.back()->locals["input"] = heap.allocate<Builtin>("input");
            stack.back()->locals["intcast"] = heap.allocate<Builtin>("intcast");
            
        }

        void visit(ast::Identifier &identifier) {
            rVal_ = stack.back()->lookup_read(identifier.name);
        };
        void visit(ast::Block &block) {
            block.left->accept(*this);
            if (rVal_->as_return() != nullptr) {
                return;
            }
            block.right->accept(*this);
            return;
        };
        void visit(ast::Closure &closure) {
            closure.body->accept(*this);

        };
        void visit(ast::Global &global) {
            (void)global;
            return;
        };
        void visit(ast::If &if_) {
            if_.condition->accept(*this);
            Value *condition = rVal_;
            if (auto *condition_bool = condition->as_boolean()) {
                if (*condition_bool) {
                    if_.then->accept(*this);
                } else {
                    if_.else_->accept(*this);
                }
            } else {
                std::cout << "IllegalCastException" << std::endl;
                std::exit(2);
            }
        };
        void visit(ast::While &while_) {
            while (true) {
                while_.condition->accept(*this);
                Value *condition = rVal_;
                if (auto *condition_bool = condition->as_boolean()) {
                    if (*condition_bool) {
                        while_.body->accept(*this);
                    } else {
                        break;
                    }
                } else {
                    std::cout << "IllegalCastException" << std::endl;
                    std::exit(2);
                }
            }
        };
        void visit(ast::Return &return_) {
            return_.expr->accept(*this);
            auto value = rVal_;
            rVal_ = heap.allocate<Return>(value);
            return;
        };
        void visit(ast::Assignment &assignment) {
            assignment.value->accept(*this);
            auto rhs = rVal_;
            if (auto lhs_id = dynamic_cast<ast::Identifier*>(assignment.target.get())) {
                auto name = lhs_id->name;
                Frame* frame = stack.back()->lookup_write(name);
                frame->locals[name] = rhs;
                return;
            } else if (auto lhs_fd = dynamic_cast<ast::FieldDereference*>(assignment.target.get())) {
                lhs_fd->base->accept(*this);
                auto record = rVal_;
                if (auto r_record = record->as_record()) {
                    r_record->fields[lhs_fd->field->name] = rhs;
                    return;
                } else {
                    std::cout << "IllegalCastException" << std::endl;
                    std::exit(4);
                }
            } else if (auto lhs_ie = dynamic_cast<ast::IndexExpression*>(assignment.target.get())) {
                lhs_ie->base->accept(*this);
                auto record = rVal_;
                if (auto r_record = record->as_record()) {
                    lhs_ie->index->accept(*this);
                    auto index_value = rVal_;
                    r_record->fields[index_value->to_string()] = rhs;
                    return;
                } else {
                    std::cout << "IllegalCastException" << std::endl;
                    std::exit(4);
                }
            } else {
                std::cout << "IllegalCastException" << std::endl;
                std::exit(3);
            }
            //if lhs is an element of a record, we need the record itself
            //if lhs is an identifier, we need the return from calling lookup->write
            // if (auto record = lhs->as_record())
            std::cout << "Assignment not implemented" << std::endl;
            std::exit(4);
        };
        void visit(ast::Call &call) {
            call.location->accept(*this);
            auto *func = rVal_;
            std::vector<Value*> cargs;
            for (const auto &arg : call.args) {
                arg->accept(*this);
                auto val = rVal_;
                cargs.emplace_back(val);
            }

            if (auto *function = func->as_function()) {
                if (cargs.size() != function->args.size()) {
                    std::cout << "RuntimeException" << std::endl;
                    std::exit(3);
                }
                Frame* new_frame = heap.allocate<Frame>(function->frame, stack.front());
                globals(new_frame, function->body->body.get());
                assigns(new_frame, function->body->body.get());
                for (size_t i = 0; i < cargs.size(); i++) {
                    new_frame->locals[function->args[i]] = cargs[i];
                }
                this->stack.push_back(new_frame);
                function->body->accept(*this);
                this->stack.pop_back();
            } else if (auto *builtin = func->as_builtin()) {
                if (builtin->name == "print") {
                    if (cargs.size() != 1) {
                        std::cout << "RuntimeException" << std::endl;
                        std::exit(1);
                    } else {
                        std::cout << cargs[0]->to_string() << std::endl;
                    }
                } else if (builtin->name == "input") {
                    if (cargs.size() != 0) {
                        std::cout << "RuntimeException" << std::endl;
                        std::exit(1);
                    } else {
                        std::string in; 
                        std::getline(std::cin, in);
                        rVal_ = heap.allocate<Return>(heap.allocate<String>(in));
                    }
                } else if (builtin->name == "intcast") {
                    if (cargs.size() != 1) {
                        std::cout << "RuntimeException" << std::endl;
                        std::exit(1);
                    } else {
                        if (auto str = cargs[0]->as_string()) {
                            int val = std::atoi(str->c_str());
                            if (!val && *str != "0") {
                                std::cout << "IllegalCastException (or you tried to intcast a string rep of 0)" << std::endl;
                                std::exit(5);
                            } else {
                                rVal_ = heap.allocate<Return>(heap.allocate<Integer>(val));
                            }
                        }
                    } 
                }
            } else {
                std::cout << "IllegalCastException" << std::endl;
                std::exit(1);
            }
            if (auto ret = rVal_->as_return()) {
                rVal_ = ret->value;
            } else {
                rVal_ = this->none;
            }
            return;
            
        };
        void visit(ast::FunctionDecl &function_decl) {
            std::vector<std::string> args;
            Frame* frame = stack.back();
            ast::Closure* body = function_decl.body.get();
            for (const auto &arg : function_decl.args) {
                args.push_back(arg->name);
            }
            rVal_ = heap.allocate<Function>(frame, body, args);
        };
        void visit(ast::Record &record) {
            std::unordered_map<std::string, Value*> fields;
            for (const auto &[key, value] : record.fields) {
                value->accept(*this);
                fields[key] = rVal_;
            }
            rVal_ = heap.allocate<Record>(fields);
        };
        void visit(ast::BinOp &binop) {
            binop.lhs->accept(*this);
            auto lhs = rVal_;
            binop.rhs->accept(*this);
            auto rhs = rVal_;
            switch (binop.op) {
                case ast::BinaryOp::ADD : 
                    if (auto *left_str = lhs->as_string()) {
                        rVal_ = heap.allocate<String>(*left_str + rhs->to_string());
                    } else if (auto *right_str = rhs->as_string()) {
                        rVal_ = heap.allocate<String>(lhs->to_string() + *right_str);
                    } else if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Integer>(*left + *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    }
                    break;
                case ast::BinaryOp::SUB : 
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Integer>(*left - *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::MUL : 
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Integer>(*left * *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::DIV : 
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            if (*right == 0) {
                                std::cout << "IllegalArithmeticException" << std::endl;
                                std::exit(2);
                            }
                            rVal_ = heap.allocate<Integer>(*left / *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::LE :
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Boolean>(*left <= *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::LT :
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Boolean>(*left < *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::GE :
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Boolean>(*left >= *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::GT :
                    if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Boolean>(*left > *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::EQ :
                    if (auto *left = lhs->as_boolean()) {
                        if (auto *right = rhs->as_boolean()) {
                            rVal_ = heap.allocate<Boolean>(*left == *right);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else if (auto *left = lhs->as_integer()) {
                        if (auto *right = rhs->as_integer()) {
                            rVal_ = heap.allocate<Boolean>(*left == *right);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else if (auto *left = lhs->as_string()) {
                        if (auto *right = rhs->as_string()) {
                            rVal_ = heap.allocate<Boolean>(*left == *right);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else if (lhs->as_none()) {
                        if (rhs->as_none()) {
                            rVal_ = heap.allocate<Boolean>(true);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else if (auto *left = lhs->as_record()) {
                        if (auto *right = rhs->as_record()) {
                            rVal_ = heap.allocate<Boolean>(left == right);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else if (auto *left = lhs->as_function()) {
                        if (auto *right = rhs->as_function()) {
                            rVal_ = heap.allocate<Boolean>(left == right);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else if (auto *left = lhs->as_builtin()) {
                        if (auto *right = rhs->as_builtin()) {
                            rVal_ = heap.allocate<Boolean>(left == right);
                        } else {
                            rVal_ = heap.allocate<Boolean>(false);
                        }
                    } else {
                        std::cout << "Didn't know we could hit this case";
                        rVal_ = heap.allocate<Boolean>(false);
                    }
                    break;
                case ast::BinaryOp::OR :
                    if (auto *left = lhs->as_boolean()) {
                        if (auto *right = rhs->as_boolean()) {
                            rVal_ = heap.allocate<Boolean>(*left || *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                case ast::BinaryOp::AND :
                    if (auto *left = lhs->as_boolean()) {
                        if (auto *right = rhs->as_boolean()) {
                            rVal_ = heap.allocate<Boolean>(*left && *right);
                        } else {
                            std::cout << "IllegalCastException" << std::endl;
                            std::exit(2);
                        }
                    } else {
                        std::cout << "IllegalCastException" << std::endl;
                        std::exit(2);
                    }
                    break;
                default:
                    std::cout << "This cannot be";
                    std::exit(3);
            }
        };
        void visit(ast::FieldDereference &field_deref) {
            field_deref.base->accept(*this);
            auto *record = rVal_;
            if (auto r_record = record->as_record()) {
                if (r_record->fields.find(field_deref.field->name) == r_record->fields.end()) {
                    rVal_ = this->none;
                    return;
                }
                rVal_ = r_record->fields[field_deref.field->name];
                return;
            }
            std::cout << "IllegalCastException" << std::endl;
            std::exit(2);
        };
        void visit(ast::IndexExpression &idx_expr) {
            idx_expr.base->accept(*this);
            auto *record = rVal_;
            if (auto r_record = record->as_record()) {
                idx_expr.index->accept(*this);
                std::string idx_name = rVal_->to_string();
                if (r_record->fields.find(idx_name) == r_record->fields.end()) {
                    rVal_ = this->none;
                    return;
                }
                rVal_ = r_record->fields[idx_name];
                return;
            }
            std::cout << "IllegalCastException" << std::endl;
            std::exit(2);
        };
        void visit(ast::UnOp &unop) {
            unop.expr->accept(*this);
            auto value = rVal_;
            if (unop.op == ast::UnaryOp::NOT) {
                if (auto *value_bool = value->as_boolean()) {
                    rVal_ = heap.allocate<Boolean>(!(*value_bool));
                } else {
                    std::cout << "IllegalCastException" << std::endl;
                    std::exit(2);
                }
            } else if (unop.op == ast::UnaryOp::NEG) {
                if (auto *value_int = value->as_integer()) {
                    rVal_ = heap.allocate<Integer>(-*value_int);
                } else {
                    std::cout << "IllegalCastException" << std::endl;
                    std::exit(2);
                }
            }
            return;
        };
        void visit(ast::Integer &integer) {
            rVal_ = heap.allocate<Integer>(integer.value);
        };
        void visit(ast::String &string) {
            rVal_ = heap.allocate<String>(string.value);
        };
        void visit(ast::None &none) {
            (void)none;
            rVal_ = this->none;
        };
        void visit(ast::Boolean &boolean) {
            rVal_ = heap.allocate<Boolean>(boolean.value);
        };
        void visit(ast::Nop &nop) {
            (void)nop;
            return;
        };

        void globals(Frame*& frame, ast::ParseNode* body) {
            if (auto body_block = dynamic_cast<ast::Block*>(body)) {
                globals(frame, body_block->left.get());
                globals(frame, body_block->right.get());
            } else if (auto body_if = dynamic_cast<ast::If*>(body)) {
                globals(frame, body_if->then.get());
                globals(frame, body_if->else_.get());
            } else if (auto body_while = dynamic_cast<ast::While*>(body)) {
                globals(frame, body_while->body.get());
            } else if (auto body_global = dynamic_cast<ast::Global*>(body)) {
                frame->globals.insert(body_global->name);
            }
            return;
        };

        void assigns(Frame*& frame, ast::ParseNode* body) {
            if (auto body_block = dynamic_cast<ast::Block*>(body)) {
                assigns(frame, body_block->left.get());
                assigns(frame, body_block->right.get());
            } else if (auto body_if = dynamic_cast<ast::If*>(body)) {
                assigns(frame, body_if->then.get());
                assigns(frame, body_if->else_.get());
            } else if (auto body_while = dynamic_cast<ast::While*>(body)) {
                assigns(frame, body_while->body.get());
            } else if (auto body_assign = dynamic_cast<ast::Assignment*>(body)) {
                if (auto id = dynamic_cast<ast::Identifier*>((*body_assign).target.get())) {
                    frame->locals[id->name] = this->none;
                }
            }
            return;
        };
};

void interpretcode::interpret(std::string &file_contents, size_t memLimit) {
    size_t memLimitBytes = memLimit * 1024 * 1024;
    Interpreter interpreter(memLimitBytes);
    (*parsecode::parse(file_contents)).accept(interpreter);
    return;
}
