#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../gc/gc.hpp"
#include "./types.hpp"

namespace bytecode
{
  // Forward declarations
  class VirtualMachine;
  struct VMValue;
  struct Value;
  struct StackFrame;

  // Runtime immediate/boxed value representation
  struct Value
  {
    enum class Tag : uint8_t
    {
      Uninitialized,
      None,
      Boolean,
      Integer,
      HeapPtr,
    };

    Tag tag;
    union
    {
      bool b;
      int32_t i;
      VMValue *heap;
    };

    static Value uninitialized()
    {
      Value v;
      v.tag = Tag::Uninitialized;
      v.heap = nullptr;
      return v;
    }

    static Value none()
    {
      Value v;
      v.tag = Tag::None;
      v.heap = nullptr;
      return v;
    }

    static Value boolean(bool x)
    {
      Value v;
      v.tag = Tag::Boolean;
      v.b = x;
      return v;
    }

    static Value integer(int32_t x)
    {
      Value v;
      v.tag = Tag::Integer;
      v.i = x;
      return v;
    }

    static Value heapPtr(VMValue *p)
    {
      Value v;
      v.tag = Tag::HeapPtr;
      v.heap = p;
      return v;
    }

    bool isUninitialized() const { return tag == Tag::Uninitialized; }
  };

  // Heap-allocated runtime objects (GC-managed)
  struct VMValue : public Collectable
  {
    enum class Type
    {
      String,
      Record,
      Function,
      Closure,
      Reference,
    };

    virtual ~VMValue() = default;
    virtual Type getType() const = 0;
  };

  // String value
  struct VMString : public VMValue
  {
    std::string value;

    VMString(const std::string &v) : value(v) {}

    Type getType() const override { return Type::String; }

    size_t getSize() override
    {
      return sizeof(VMString) + value.capacity();
    }

    void follow(CollectedHeap &) override
    {
      // no heap pointers inside
    }
  };

  // Record value (map from strings to values)
  struct VMRecord : public VMValue
  {
    // Records can hold immediate or heap values
    std::unordered_map<std::string, Value> fields;

    Type getType() const override { return Type::Record; }

    size_t getSize() override
    {
      return sizeof(VMRecord) +
             fields.size() * (sizeof(std::string) + sizeof(Value));
    }

    void follow(CollectedHeap &heap) override
    {
      for (auto &pair : fields)
      {
        Value &v = pair.second;
        if (v.tag == Value::Tag::HeapPtr && v.heap)
        {
          heap.markSuccessors(v.heap);
        }
      }
    }
  };

  // Function value (bytecode function)
  struct VMFunction : public VMValue
  {
    Function *function;

    VMFunction(Function *f) : function(f) {}

    Type getType() const override { return Type::Function; }

    size_t getSize() override { return sizeof(VMFunction); }

    void follow(CollectedHeap &) override
    {
      // Function* is not GC-managed; it’s owned by the compiler/front-end
    }
  };

  // Reference value (reference to a variable slot holding a Value)
  struct VMReference : public VMValue
  {
    Value *ref; // Pointer to storage slot containing a Value

    VMReference(Value *r) : ref(r) {}

    Type getType() const override { return Type::Reference; }

    size_t getSize() override { return sizeof(VMReference); }

    void follow(CollectedHeap &heap) override
    {
      if (ref && ref->tag == Value::Tag::HeapPtr && ref->heap)
      {
        heap.markSuccessors(ref->heap);
      }
    }
  };

  // Closure value (function + captured variable references)
  struct VMClosure : public VMValue
  {
    VMFunction *function;
    std::vector<VMReference *> free_vars;

    VMClosure(VMFunction *f, const std::vector<VMReference *> &fv)
        : function(f), free_vars(fv) {}

    Type getType() const override { return Type::Closure; }

    size_t getSize() override
    {
      return sizeof(VMClosure) + free_vars.size() * sizeof(VMReference *);
    }

    void follow(CollectedHeap &heap) override
    {
      if (function)
      {
        heap.markSuccessors(function);
      }
      for (auto *ref : free_vars)
      {
        if (ref)
        {
          heap.markSuccessors(ref);
        }
      }
    }
  };


  // Stack frame for function execution (GC-managed root)
  struct StackFrame : public Collectable
  {
    // Instruction pointer (current position in instructions)
    size_t ip;

    // The function being executed
    Function *function;

    // Local variables (indexed by local_vars)
    std::vector<Value> locals;

    // Heap-allocated storage for referenceable local variables
    // Each Value* points to a separately allocated slot
    std::vector<Value *> local_ref_storage;

    // Local variable reference slots (same order as function->local_reference_vars_)
    std::vector<Value *> local_refs;

    // Free variable references (captured from parent scope)
    std::vector<VMReference *> free_var_refs;

    // Operand stack for this frame
    std::vector<Value> operand_stack;

    StackFrame(Function *f, const std::vector<VMReference *> &free_refs);

    size_t getSize() override;

    void follow(CollectedHeap &heap) override;
  };

  // Virtual Machine
  class VirtualMachine
  {
  private:
    CollectedHeap &heap_;

    // Global variables: indexed by name index (Function::names_)
	std::unordered_map<std::string, Value> globals_;  // No need for global_name_to_index_

    // Call stack
    std::vector<StackFrame *> call_stack_;

    // Reusable buffers to avoid per-call allocations
    std::vector<Value> call_args_;
    std::vector<VMReference *> call_free_vars_;

    // Native function implementations
    Value nativePrint(const std::vector<Value> &args);
    Value nativeInput(const std::vector<Value> &args);
    Value nativeIntcast(const std::vector<Value> &args);

    // Inline Helper methods for instruction execution
    inline StackFrame *currentFrame()
    {
      if (call_stack_.empty())
      {
        throw std::runtime_error("RuntimeException: No active frame");
      }
      return call_stack_.back();
    }

    inline Value pop()
    {
      StackFrame *frame = currentFrame();
      if (frame->operand_stack.empty())
      {
        throw std::runtime_error("InsufficientStackException: Cannot pop from empty stack");
      }
      Value val = frame->operand_stack.back();
      frame->operand_stack.pop_back();
      return val;
    }

    inline void push(const Value &value)
    {
      StackFrame *frame = currentFrame();
      frame->operand_stack.push_back(value);
    }

    inline Value &peek(size_t offset = 0)
    {
      StackFrame *frame = currentFrame();
      if (frame->operand_stack.size() <= offset)
      {
        throw std::runtime_error("InsufficientStackException: Stack underflow");
      }
      return frame->operand_stack[frame->operand_stack.size() - 1 - offset];
    }

    // Instruction handlers
    void executeLoadConst(int32_t operand);
    void executeLoadFunc(int32_t operand);
    void executeLoadLocal(int32_t operand);
    void executeStoreLocal(int32_t operand);
    void executeLoadGlobal(int32_t operand);
    void executeStoreGlobal(int32_t operand);
    void executePushReference(int32_t operand);
    void executeLoadReference();
    void executeStoreReference();
    void executeAllocRecord();
    void executeFieldLoad(int32_t operand);
    void executeFieldStore(int32_t operand);
    void executeIndexLoad();
    void executeIndexStore();
    void executeAllocClosure(int32_t operand);
    void executeCall(int32_t operand);
    void executeReturn();
    void executeAdd();
    void executeSub();
    void executeMul();
    void executeDiv();
    void executeNeg();
    void executeGt();
    void executeGeq();
    void executeEq();
    void executeAnd();
    void executeOr();
    void executeNot();
    void executeGoto(int32_t operand);
    void executeIf(int32_t operand);
    void executeDup();
    void executeSwap();
    void executePop();

    // GC root collection
    std::vector<Collectable *> getRoots();

  public:
    VirtualMachine(CollectedHeap &heap);
    ~VirtualMachine();

    // Execute a bytecode program
    void execute(Function *program);

    // Optional global API (string-based, for embedding)
    Value getGlobal(const std::string &name) const;
    void setGlobal(const std::string &name, const Value &value);
  };

} // namespace bytecode

