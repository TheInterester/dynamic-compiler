#include "vm.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <sstream>

namespace bytecode
{

	// StackFrame implementation
	StackFrame::StackFrame(Function *f, const std::vector<VMReference *> &free_refs)
		: ip(0), function(f), free_var_refs(free_refs)
	{
		// Initialize locals
		locals.resize(f->local_vars_.size(), Value::uninitialized());

		// Reserve for operand stack (tune if you like)
		operand_stack.reserve(64);

		// Heap-allocate storage for each referenceable local, in the same order
		// as function->local_reference_vars_ and function->local_ref_index_.
		local_ref_storage.reserve(f->local_reference_vars_.size());
		local_refs.reserve(f->local_reference_vars_.size());

		for (size_t ref_i = 0; ref_i < f->local_reference_vars_.size(); ++ref_i)
		{
			Value *slot = new Value(Value::uninitialized());
			local_ref_storage.push_back(slot);
			local_refs.push_back(slot);
		}
	}

	size_t StackFrame::getSize()
	{
		return sizeof(StackFrame) +
			   locals.size() * sizeof(Value) +
			   local_ref_storage.size() * sizeof(Value *) +
			   local_refs.size() * sizeof(Value *) +
			   free_var_refs.size() * sizeof(VMReference *) +
			   operand_stack.size() * sizeof(Value);
	}

	void StackFrame::follow(CollectedHeap &heap)
	{
		// Mark locals
		for (auto &v : locals)
		{
			if (v.tag == Value::Tag::HeapPtr && v.heap)
			{
				heap.markSuccessors(v.heap);
			}
		}

		// Mark heap-allocated storage for referenceable variables
		for (auto *slot : local_ref_storage)
		{
			if (slot && slot->tag == Value::Tag::HeapPtr && slot->heap)
			{
				heap.markSuccessors(slot->heap);
			}
		}

		// Mark operand stack
		for (auto &v : operand_stack)
		{
			if (v.tag == Value::Tag::HeapPtr && v.heap)
			{
				heap.markSuccessors(v.heap);
			}
		}

		// Mark free variable references (closures)
		for (auto *ref : free_var_refs)
		{
			if (ref)
			{
				heap.markSuccessors(ref);
			}
		}
	}

	// VirtualMachine implementation
	VirtualMachine::VirtualMachine(CollectedHeap &heap) : heap_(heap)
	{
		// Reserve call stack capacity
		call_stack_.reserve(256);

		// Set up GC roots callback
		heap_.setRoots([this]()
					   { return this->getRoots(); });
	}

	VirtualMachine::~VirtualMachine()
	{
		// StackFrames and VMValues are GC-managed; nothing to do here.
	}

	// Get roots for GC
	std::vector<Collectable *> VirtualMachine::getRoots()
	{
		std::vector<Collectable *> roots;

		for (auto *frame : call_stack_)
		{
			if (frame)
			{
				roots.push_back(frame);
			}
		}

		// All heap values reachable from globals
		for (auto &pair : globals_)
		{
			if (pair.second.tag == Value::Tag::HeapPtr && pair.second.heap)
			{
				roots.push_back(pair.second.heap);
			}
		}

		return roots;
	}

	// Optional embedding API: string-based global access
	Value VirtualMachine::getGlobal(const std::string &name) const
	{
		auto it = globals_.find(name);
		if (it == globals_.end())
		{
			throw std::runtime_error("UninitializedVariableException: Global '" + name + "' not initialized");
		}
		Value v = it->second;
		if (v.isUninitialized())
		{
			throw std::runtime_error("UninitializedVariableException: Global '" + name + "' not initialized");
		}
		return v;
	}

	void VirtualMachine::setGlobal(const std::string &name, const Value &value)
	{
		globals_[name] = value;
	}

	// Main execution loop with computed goto optimization
	void VirtualMachine::execute(Function *program)
	{
		if (!program)
		{
			throw std::runtime_error("RuntimeException: program is null");
		}

		// Create initial frame
		StackFrame *frame = heap_.allocate<StackFrame>(program, std::vector<VMReference *>());
		call_stack_.push_back(frame);

// Direct-threaded code using computed goto (GCC/Clang extension)
#if defined(__GNUC__) || defined(__clang__)
		// Dispatch table with label addresses for each operation
		static const void *dispatch_table[] = {
			&&label_LoadConst,
			&&label_LoadFunc,
			&&label_LoadLocal,
			&&label_StoreLocal,
			&&label_LoadGlobal,
			&&label_StoreGlobal,
			&&label_PushReference,
			&&label_LoadReference,
			&&label_StoreReference,
			&&label_AllocRecord,
			&&label_FieldLoad,
			&&label_FieldStore,
			&&label_IndexLoad,
			&&label_IndexStore,
			&&label_AllocClosure,
			&&label_Call,
			&&label_Return,
			&&label_Add,
			&&label_Sub,
			&&label_Mul,
			&&label_Div,
			&&label_Neg,
			&&label_Gt,
			&&label_Geq,
			&&label_Eq,
			&&label_And,
			&&label_Or,
			&&label_Not,
			&&label_Goto,
			&&label_If,
			&&label_Dup,
			&&label_Swap,
			&&label_Pop};

		// Variable to hold current instruction
		const Instruction *current_instr = nullptr;

// Macro to dispatch to next instruction
#define DISPATCH()                                                                               \
	do                                                                                           \
	{                                                                                            \
		if (call_stack_.empty())                                                                 \
			goto vm_exit;                                                                        \
		frame = currentFrame();                                                                  \
		if (frame->ip >= frame->function->instructions.size())                                   \
		{                                                                                        \
			if (call_stack_.size() > 1)                                                          \
			{                                                                                    \
				throw std::runtime_error("RuntimeException: Function missing return statement"); \
			}                                                                                    \
			push(Value::none());                                                                 \
			goto label_Return;                                                                   \
		}                                                                                        \
		current_instr = &frame->function->instructions[frame->ip];                               \
		frame->ip++;                                                                             \
		goto *dispatch_table[static_cast<int>(current_instr->operation)];                        \
	} while (0)

		// Start dispatching
		DISPATCH();

	// Instruction handlers as labels
	label_LoadConst:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeLoadConst(current_instr->operand0.value());
		DISPATCH();

	label_LoadFunc:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeLoadFunc(current_instr->operand0.value());
		DISPATCH();

	label_LoadLocal:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeLoadLocal(current_instr->operand0.value());
		DISPATCH();

	label_StoreLocal:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeStoreLocal(current_instr->operand0.value());
		DISPATCH();

	label_LoadGlobal:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeLoadGlobal(current_instr->operand0.value());
		DISPATCH();

	label_StoreGlobal:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeStoreGlobal(current_instr->operand0.value());
		DISPATCH();

	label_PushReference:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executePushReference(current_instr->operand0.value());
		DISPATCH();

	label_LoadReference:
		executeLoadReference();
		DISPATCH();

	label_StoreReference:
		executeStoreReference();
		DISPATCH();

	label_AllocRecord:
		executeAllocRecord();
		DISPATCH();

	label_FieldLoad:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeFieldLoad(current_instr->operand0.value());
		DISPATCH();

	label_FieldStore:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeFieldStore(current_instr->operand0.value());
		DISPATCH();

	label_IndexLoad:
		executeIndexLoad();
		DISPATCH();

	label_IndexStore:
		executeIndexStore();
		DISPATCH();

	label_AllocClosure:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeAllocClosure(current_instr->operand0.value());
		DISPATCH();

	label_Call:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeCall(current_instr->operand0.value());
		DISPATCH();

	label_Return:
		executeReturn();
		DISPATCH();

	label_Add:
		executeAdd();
		DISPATCH();

	label_Sub:
		executeSub();
		DISPATCH();

	label_Mul:
		executeMul();
		DISPATCH();

	label_Div:
		executeDiv();
		DISPATCH();

	label_Neg:
		executeNeg();
		DISPATCH();

	label_Gt:
		executeGt();
		DISPATCH();

	label_Geq:
		executeGeq();
		DISPATCH();

	label_Eq:
		executeEq();
		DISPATCH();

	label_And:
		executeAnd();
		DISPATCH();

	label_Or:
		executeOr();
		DISPATCH();

	label_Not:
		executeNot();
		DISPATCH();

	label_Goto:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeGoto(current_instr->operand0.value());
		DISPATCH();

	label_If:
		if (!current_instr->operand0.has_value())
			throw std::runtime_error("RuntimeException: Missing operand");
		executeIf(current_instr->operand0.value());
		DISPATCH();

	label_Dup:
		executeDup();
		DISPATCH();

	label_Swap:
		executeSwap();
		DISPATCH();

	label_Pop:
		executePop();
		DISPATCH();

	vm_exit:; // Empty statement required after label
#undef DISPATCH

#else
		// Fallback to switch-based dispatch for compilers without computed goto
		while (!call_stack_.empty())
		{
			frame = currentFrame();

			// Check if we've reached the end of instructions
			if (frame->ip >= frame->function->instructions.size())
			{
				// Only allow implicit return for the main/module function (first frame)
				if (call_stack_.size() > 1)
				{
					throw std::runtime_error("RuntimeException: Function missing return statement");
				}
				// Implicit return None at end of main program is OK
				push(Value::none());
				executeReturn();
				continue;
			}

			// Fetch instruction
			const Instruction &instr = frame->function->instructions[frame->ip];

			// Increment IP before execution (some instructions modify it)
			frame->ip++;

			// Decode and execute
			switch (instr.operation)
			{
			case Operation::LoadConst:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeLoadConst(instr.operand0.value());
				break;
			case Operation::LoadFunc:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeLoadFunc(instr.operand0.value());
				break;
			case Operation::LoadLocal:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeLoadLocal(instr.operand0.value());
				break;
			case Operation::StoreLocal:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeStoreLocal(instr.operand0.value());
				break;
			case Operation::LoadGlobal:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeLoadGlobal(instr.operand0.value());
				break;
			case Operation::StoreGlobal:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeStoreGlobal(instr.operand0.value());
				break;
			case Operation::PushReference:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executePushReference(instr.operand0.value());
				break;
			case Operation::LoadReference:
				executeLoadReference();
				break;
			case Operation::StoreReference:
				executeStoreReference();
				break;
			case Operation::AllocRecord:
				executeAllocRecord();
				break;
			case Operation::FieldLoad:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeFieldLoad(instr.operand0.value());
				break;
			case Operation::FieldStore:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeFieldStore(instr.operand0.value());
				break;
			case Operation::IndexLoad:
				executeIndexLoad();
				break;
			case Operation::IndexStore:
				executeIndexStore();
				break;
			case Operation::AllocClosure:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeAllocClosure(instr.operand0.value());
				break;
			case Operation::Call:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeCall(instr.operand0.value());
				break;
			case Operation::Return:
				executeReturn();
				break;
			case Operation::Add:
				executeAdd();
				break;
			case Operation::Sub:
				executeSub();
				break;
			case Operation::Mul:
				executeMul();
				break;
			case Operation::Div:
				executeDiv();
				break;
			case Operation::Neg:
				executeNeg();
				break;
			case Operation::Gt:
				executeGt();
				break;
			case Operation::Geq:
				executeGeq();
				break;
			case Operation::Eq:
				executeEq();
				break;
			case Operation::And:
				executeAnd();
				break;
			case Operation::Or:
				executeOr();
				break;
			case Operation::Not:
				executeNot();
				break;
			case Operation::Goto:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeGoto(instr.operand0.value());
				break;
			case Operation::If:
				if (!instr.operand0.has_value())
					throw std::runtime_error("RuntimeException: Missing operand");
				executeIf(instr.operand0.value());
				break;
			case Operation::Dup:
				executeDup();
				break;
			case Operation::Swap:
				executeSwap();
				break;
			case Operation::Pop:
				executePop();
				break;
			default:
				throw std::runtime_error("RuntimeException: Unknown operation");
			}
		}
#endif
	}

	// Instruction implementations
	void VirtualMachine::executeLoadConst(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= (int32_t)frame->function->constants_.size())
		{
			throw std::runtime_error("RuntimeException: Invalid constant index");
		}

		Constant *constant = frame->function->constants_[operand];

		if (dynamic_cast<Constant::None *>(constant))
		{
			push(Value::none());
		}
		else if (auto *integer = dynamic_cast<Constant::Integer *>(constant))
		{
			push(Value::integer(integer->value));
		}
		else if (auto *str = dynamic_cast<Constant::String *>(constant))
		{
			VMString *s = heap_.allocate<VMString>(str->value);
			push(Value::heapPtr(s));
		}
		else if (auto *boolean = dynamic_cast<Constant::Boolean *>(constant))
		{
			push(Value::boolean(boolean->value));
		}
		else
		{
			throw std::runtime_error("RuntimeException: Unknown constant type");
		}
	}

	void VirtualMachine::executeLoadFunc(int32_t operand)
	{
		StackFrame *frame = currentFrame();

		if (!frame->function)
		{
			throw std::runtime_error("RuntimeException: frame->function is null!");
		}

		if (operand < 0 || operand >= (int32_t)frame->function->functions_.size())
		{
			throw std::runtime_error("RuntimeException: Invalid function index");
		}

		Function *func = frame->function->functions_[operand];
		VMFunction *vf = heap_.allocate<VMFunction>(func);
		push(Value::heapPtr(vf));
	}

	void VirtualMachine::executeLoadLocal(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= static_cast<int32_t>(frame->locals.size()))
		{
			throw std::runtime_error("RuntimeException: Invalid local variable index");
		}

		// O(1) lookup using precomputed local_ref_index_
		int32_t ref_idx = frame->function->local_ref_index_[operand];

		Value val;
		if (ref_idx >= 0)
		{
			// reference-backed local
			Value *slot = frame->local_refs[ref_idx];
			if (!slot || slot->isUninitialized())
			{
				throw std::runtime_error(
					"UninitializedVariableException: Local reference not initialized");
			}
			val = *slot;
		}
		else
		{
			// plain local
			val = frame->locals[operand];
			if (val.isUninitialized())
			{
				throw std::runtime_error(
					"UninitializedVariableException: Local variable not initialized");
			}
		}

		push(val);
	}

	void VirtualMachine::executeStoreLocal(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= static_cast<int32_t>(frame->locals.size()))
		{
			throw std::runtime_error("RuntimeException: Invalid local variable index");
		}

		Value val = pop();

		// O(1) lookup using precomputed local_ref_index_
		int32_t ref_idx = frame->function->local_ref_index_[operand];

		if (ref_idx >= 0)
		{
			// this local is referenceable: write into the canonical slot
			Value *slot = frame->local_refs[ref_idx];
			if (!slot)
			{
				throw std::runtime_error("RuntimeException: Null local_refs slot");
			}
			*slot = val;
		}
		else
		{
			// plain local
			frame->locals[operand] = val;
		}
	}

	void VirtualMachine::executeLoadGlobal(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= (int32_t)frame->function->names_.size())
		{
			throw std::runtime_error("RuntimeException: Invalid global name index");
		}

		const std::string &name = frame->function->names_[operand];

		auto it = globals_.find(name);
		if (it == globals_.end())
		{
			throw std::runtime_error("UninitializedVariableException: Global variable '" +
									 name + "' not initialized");
		}

		Value v = it->second;
		if (v.isUninitialized())
		{
			throw std::runtime_error("UninitializedVariableException: Global '" + name + "' not initialized");
		}

		push(v);
	}

	void VirtualMachine::executeStoreGlobal(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= (int32_t)frame->function->names_.size())
		{
			throw std::runtime_error("RuntimeException: Invalid global name index");
		}

		const std::string &name = frame->function->names_[operand];
		Value val = pop();
		globals_[name] = val;
	}

	void VirtualMachine::executePushReference(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (!frame)
		{
			throw std::runtime_error("RuntimeException: No current frame in PushReference");
		}

		Value *ref_ptr = nullptr;

		int32_t local_ref_count =
			static_cast<int32_t>(frame->local_refs.size());

		if (operand < local_ref_count)
		{
			// Local-by-ref slot in *this* frame
			ref_ptr = frame->local_refs[operand];
			if (!ref_ptr)
			{
				throw std::runtime_error(
					"RuntimeException: Null local reference slot");
			}
		}
		else
		{
			// Free variable captured from an outer frame
			int32_t free_idx = operand - local_ref_count;
			if (free_idx < 0 ||
				free_idx >= static_cast<int32_t>(frame->free_var_refs.size()))
			{
				throw std::runtime_error("RuntimeException: Invalid reference index");
			}

			VMReference *outer_ref = frame->free_var_refs[free_idx];
			if (!outer_ref || !outer_ref->ref)
			{
				throw std::runtime_error(
					"RuntimeException: Invalid outer reference slot");
			}

			ref_ptr = outer_ref->ref;
		}

		VMReference *ref_obj = heap_.allocate<VMReference>(ref_ptr);
		push(Value::heapPtr(ref_obj));
	}

	void VirtualMachine::executeLoadReference()
	{
		Value v = pop();
		if (v.tag != Value::Tag::HeapPtr || !v.heap || v.heap->getType() != VMValue::Type::Reference)
		{
			throw std::runtime_error("RuntimeException: Expected reference value");
		}

		VMReference *ref = static_cast<VMReference *>(v.heap);
		if (!ref->ref || ref->ref->isUninitialized())
		{
			throw std::runtime_error("UninitializedVariableException: Referenced variable not initialized");
		}

		push(*ref->ref);
	}

	void VirtualMachine::executeStoreReference()
	{
		Value val = pop();
		Value ref_val = pop();

		if (ref_val.tag != Value::Tag::HeapPtr || !ref_val.heap || ref_val.heap->getType() != VMValue::Type::Reference)
		{
			throw std::runtime_error("RuntimeException: Expected reference value");
		}

		VMReference *ref = static_cast<VMReference *>(ref_val.heap);
		if (!ref->ref)
		{
			throw std::runtime_error("RuntimeException: Null reference slot");
		}
		*ref->ref = val;
	}

	void VirtualMachine::executeAllocRecord()
	{
		VMRecord *rec = heap_.allocate<VMRecord>();
		push(Value::heapPtr(rec));
	}

	void VirtualMachine::executeFieldLoad(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= (int32_t)frame->function->names_.size())
		{
			throw std::runtime_error("RuntimeException: Invalid field name index");
		}

		const std::string &field_name = frame->function->names_[operand];
		Value record_val = pop();

		if (record_val.tag != Value::Tag::HeapPtr || !record_val.heap ||
			record_val.heap->getType() != VMValue::Type::Record)
		{
			throw std::runtime_error("RuntimeException: Expected record value");
		}

		VMRecord *record = static_cast<VMRecord *>(record_val.heap);
		auto it = record->fields.find(field_name);
		if (it == record->fields.end())
		{
			push(Value::none());
		}
		else
		{
			push(it->second);
		}
	}

	void VirtualMachine::executeFieldStore(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		if (operand < 0 || operand >= (int32_t)frame->function->names_.size())
		{
			throw std::runtime_error("RuntimeException: Invalid field name index");
		}

		const std::string &field_name = frame->function->names_[operand];
		Value val = pop();
		Value record_val = pop();

		if (record_val.tag != Value::Tag::HeapPtr || !record_val.heap ||
			record_val.heap->getType() != VMValue::Type::Record)
		{
			throw std::runtime_error("RuntimeException: Expected record value");
		}

		VMRecord *record = static_cast<VMRecord *>(record_val.heap);
		record->fields[field_name] = val;
	}

	void VirtualMachine::executeIndexLoad()
	{
		Value index_val = pop();
		Value record_val = pop();

		if (record_val.tag != Value::Tag::HeapPtr || !record_val.heap ||
			record_val.heap->getType() != VMValue::Type::Record)
		{
			throw std::runtime_error("RuntimeException: Expected record value");
		}

		std::string index_str;
		if (index_val.tag == Value::Tag::Integer)
		{
			index_str = std::to_string(index_val.i);
		}
		else if (index_val.tag == Value::Tag::HeapPtr && index_val.heap &&
				 index_val.heap->getType() == VMValue::Type::String)
		{
			index_str = static_cast<VMString *>(index_val.heap)->value;
		}
		else
		{
			throw std::runtime_error("RuntimeException: Index must be integer or string");
		}

		VMRecord *record = static_cast<VMRecord *>(record_val.heap);
		auto it = record->fields.find(index_str);
		if (it == record->fields.end())
		{
			push(Value::none());
		}
		else
		{
			push(it->second);
		}
	}

	void VirtualMachine::executeIndexStore()
	{
		Value val = pop();
		Value index_val = pop();
		Value record_val = pop();

		if (record_val.tag != Value::Tag::HeapPtr || !record_val.heap ||
			record_val.heap->getType() != VMValue::Type::Record)
		{
			throw std::runtime_error("RuntimeException: Expected record value");
		}

		std::string index_str;
		if (index_val.tag == Value::Tag::Integer)
		{
			index_str = std::to_string(index_val.i);
		}
		else if (index_val.tag == Value::Tag::HeapPtr && index_val.heap &&
				 index_val.heap->getType() == VMValue::Type::String)
		{
			index_str = static_cast<VMString *>(index_val.heap)->value;
		}
		else
		{
			throw std::runtime_error("RuntimeException: Index must be integer or string");
		}

		VMRecord *record = static_cast<VMRecord *>(record_val.heap);
		record->fields[index_str] = val;
	}

	void VirtualMachine::executeAllocClosure(int32_t operand)
	{
		// DON'T pop yet - keep values on stack as GC roots!
		// Peek at function (it's at position operand on the stack)
		Value &func_val = peek(operand);
		if (func_val.tag != Value::Tag::HeapPtr || !func_val.heap ||
			func_val.heap->getType() != VMValue::Type::Function)
		{
			throw std::runtime_error("RuntimeException: Expected function for closure allocation");
		}
		VMFunction *func_obj = static_cast<VMFunction *>(func_val.heap);

		// Collect free variable references (also peeking, not popping)
		call_free_vars_.clear();
		call_free_vars_.reserve(operand);
		for (int32_t i = 0; i < operand; ++i)
		{
			Value &ref_val = peek(operand - 1 - i);
			if (ref_val.tag != Value::Tag::HeapPtr || !ref_val.heap ||
				ref_val.heap->getType() != VMValue::Type::Reference)
			{
				throw std::runtime_error("RuntimeException: Expected reference for closure free var");
			}
			call_free_vars_.push_back(static_cast<VMReference *>(ref_val.heap));
		}

		// Allocate closure (GC-safe: args still on stack)
		VMClosure *closure = heap_.allocate<VMClosure>(func_obj, call_free_vars_);

		// Now pop function + free vars
		for (int32_t i = 0; i <= operand; ++i)
		{
			pop();
		}

		// Push closure
		push(Value::heapPtr(closure));
	}

	void VirtualMachine::executeCall(int32_t operand)
	{
		// Peek args (GC roots)
		call_args_.clear();
		call_args_.reserve(operand);
		for (int32_t i = 0; i < operand; ++i)
		{
			call_args_.push_back(peek(operand - 1 - i));
		}

		// Get callable (below args)
		Value callable_val = peek(operand);
		if (callable_val.tag != Value::Tag::HeapPtr || !callable_val.heap)
		{
			throw std::runtime_error("RuntimeException: Expected callable");
		}

		Function *func = nullptr;
		std::vector<VMReference *> free_vars;

		if (callable_val.heap->getType() == VMValue::Type::Closure)
		{
			VMClosure *closure = static_cast<VMClosure *>(callable_val.heap);
			func = closure->function->function;
			free_vars = closure->free_vars;
		}
		else if (callable_val.heap->getType() == VMValue::Type::Function)
		{
			VMFunction *vf = static_cast<VMFunction *>(callable_val.heap);
			func = vf->function;
		}
		else
		{
			throw std::runtime_error("RuntimeException: Expected function or closure for call");
		}

		bool is_native = func->instructions.empty();

		if (is_native)
		{
			// Identify which native by module->functions_ index
			Function *module = call_stack_[0]->function;
			Value result;

			if (module->functions_.size() >= 3 && func == module->functions_[1])
			{
				// input()
				result = nativeInput(call_args_);
			}
			else if (module->functions_.size() >= 3 && func == module->functions_[2])
			{
				// intcast(x)
				result = nativeIntcast(call_args_);
			}
			else
			{
				// print(...)
				result = nativePrint(call_args_);
			}

			// Pop args + callable
			for (int32_t i = 0; i <= operand; ++i)
			{
				pop();
			}

			// Push result
			push(result);
			return;
		}

		// Regular user-defined function
		StackFrame *new_frame = heap_.allocate<StackFrame>(func, free_vars);

		if ((int32_t)call_args_.size() != (int32_t)func->parameter_count_)
		{
			throw std::runtime_error("RuntimeException: Wrong number of arguments");
		}

		// Initialize parameters (first parameter_count_ locals)
		for (size_t i = 0; i < call_args_.size(); ++i)
		{
			// O(1) lookup using precomputed local_ref_index_
			int32_t ref_idx = func->local_ref_index_[i];

			if (ref_idx >= 0)
			{
				// Store in heap-allocated storage
				*new_frame->local_refs[ref_idx] = call_args_[i];
			}
			else
			{
				// Regular parameter
				new_frame->locals[i] = call_args_[i];
			}
		}
		// Pop args + callable from caller stack
		for (int32_t i = 0; i <= operand; ++i)
		{
			pop();
		}

		call_stack_.push_back(new_frame);
	}

	void VirtualMachine::executeReturn()
	{
		if (call_stack_.empty())
		{
			throw std::runtime_error("RuntimeException: Cannot return from empty call stack");
		}

		Value return_val = pop();
		call_stack_.pop_back();

		if (!call_stack_.empty())
		{
			push(return_val);
		}
	}

	void VirtualMachine::executeAdd()
	{
		Value right = pop();
		Value left = pop();

		// Integer addition
		if (left.tag == Value::Tag::Integer && right.tag == Value::Tag::Integer)
		{
			int32_t result = left.i + right.i;
			push(Value::integer(result));
			return;
		}

		// String concatenation if either is string
		auto toString = [this](const Value &v) -> std::string
		{
			if (v.tag == Value::Tag::HeapPtr && v.heap)
			{
				switch (v.heap->getType())
				{
				case VMValue::Type::String:
					return static_cast<VMString *>(v.heap)->value;
				case VMValue::Type::Function:
				case VMValue::Type::Closure:
					return "FUNCTION";
				case VMValue::Type::Record:
					return "<record>";
				case VMValue::Type::Reference:
					return "<ref>";
				}
			}
			if (v.tag == Value::Tag::Integer)
				return std::to_string(v.i);
			if (v.tag == Value::Tag::Boolean)
				return v.b ? "true" : "false";
			if (v.tag == Value::Tag::None)
				return "None";
			return "<value>";
		};

		bool left_is_string = (left.tag == Value::Tag::HeapPtr && left.heap &&
							   left.heap->getType() == VMValue::Type::String);
		bool right_is_string = (right.tag == Value::Tag::HeapPtr && right.heap &&
								right.heap->getType() == VMValue::Type::String);

		if (left_is_string || right_is_string)
		{
			std::string res = toString(left) + toString(right);
			VMString *s = heap_.allocate<VMString>(res);
			push(Value::heapPtr(s));
			return;
		}

		throw std::runtime_error("IllegalCastException: Cannot add these types");
	}

	void VirtualMachine::executeSub()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Integer || right.tag != Value::Tag::Integer)
		{
			throw std::runtime_error("IllegalCastException: Expected integers");
		}

		push(Value::integer(left.i - right.i));
	}

	void VirtualMachine::executeMul()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Integer || right.tag != Value::Tag::Integer)
		{
			throw std::runtime_error("IllegalCastException: Expected integers");
		}

		push(Value::integer(left.i * right.i));
	}

	void VirtualMachine::executeDiv()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Integer || right.tag != Value::Tag::Integer)
		{
			throw std::runtime_error("IllegalCastException: Expected integers");
		}

		if (right.i == 0)
		{
			throw std::runtime_error("IllegalArithmeticException: Division by zero");
		}

		push(Value::integer(left.i / right.i));
	}

	void VirtualMachine::executeNeg()
	{
		Value v = pop();

		if (v.tag != Value::Tag::Integer)
		{
			throw std::runtime_error("IllegalCastException: Expected integer");
		}

		push(Value::integer(-v.i));
	}

	void VirtualMachine::executeGt()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Integer || right.tag != Value::Tag::Integer)
		{
			throw std::runtime_error("IllegalCastException: Expected integers");
		}

		push(Value::boolean(left.i > right.i));
	}

	void VirtualMachine::executeGeq()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Integer || right.tag != Value::Tag::Integer)
		{
			throw std::runtime_error("IllegalCastException: Expected integers");
		}

		push(Value::boolean(left.i >= right.i));
	}

	void VirtualMachine::executeEq()
	{
		Value right = pop();
		Value left = pop();

		bool result = false;

		if (left.tag != right.tag)
		{
			result = false;
		}
		else
		{
			switch (left.tag)
			{
			case Value::Tag::Uninitialized:
				result = false;
				break;
			case Value::Tag::None:
				result = true;
				break;
			case Value::Tag::Boolean:
				result = (left.b == right.b);
				break;
			case Value::Tag::Integer:
				result = (left.i == right.i);
				break;
			case Value::Tag::HeapPtr:
				if (!left.heap || !right.heap)
				{
					result = (left.heap == right.heap);
					break;
				}
				if (left.heap->getType() == VMValue::Type::String &&
					right.heap->getType() == VMValue::Type::String)
				{
					result = static_cast<VMString *>(left.heap)->value ==
							 static_cast<VMString *>(right.heap)->value;
				}
				else
				{
					// records/functions/closures/refs: compare by pointer identity
					result = (left.heap == right.heap);
				}
				break;
			}
		}

		push(Value::boolean(result));
	}

	void VirtualMachine::executeAnd()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Boolean || right.tag != Value::Tag::Boolean)
		{
			throw std::runtime_error("IllegalCastException: Expected booleans");
		}

		push(Value::boolean(left.b && right.b));
	}

	void VirtualMachine::executeOr()
	{
		Value right = pop();
		Value left = pop();

		if (left.tag != Value::Tag::Boolean || right.tag != Value::Tag::Boolean)
		{
			throw std::runtime_error("IllegalCastException: Expected booleans");
		}

		push(Value::boolean(left.b || right.b));
	}

	void VirtualMachine::executeNot()
	{
		Value v = pop();

		if (v.tag != Value::Tag::Boolean)
		{
			throw std::runtime_error("IllegalCastException: Expected boolean");
		}

		push(Value::boolean(!v.b));
	}

	void VirtualMachine::executeGoto(int32_t operand)
	{
		StackFrame *frame = currentFrame();
		frame->ip = frame->ip - 1 + operand;
	}

	void VirtualMachine::executeIf(int32_t operand)
	{
		Value cond = pop();

		if (cond.tag != Value::Tag::Boolean)
		{
			throw std::runtime_error("IllegalCastException: Expected boolean condition");
		}

		if (cond.b)
		{
			StackFrame *frame = currentFrame();
			frame->ip = frame->ip - 1 + operand;
		}
	}

	void VirtualMachine::executeDup()
	{
		Value &v = peek();
		push(v);
	}

	void VirtualMachine::executeSwap()
	{
		Value top = pop();
		Value second = pop();
		push(top);
		push(second);
	}

	void VirtualMachine::executePop()
	{
		(void)pop();
	}

	// Native functions
	Value VirtualMachine::nativePrint(const std::vector<Value> &args)
	{
		if (args.empty())
		{
			throw std::runtime_error("RuntimeException: print requires at least one argument");
		}

		// Declare first
		std::function<void(const Value &)> printValue;

		// Then define the lambda that can call itself
		printValue = [&](const Value &v)
		{
			if (v.tag == Value::Tag::Integer)
			{
				std::cout << v.i;
			}
			else if (v.tag == Value::Tag::Boolean)
			{
				std::cout << (v.b ? "true" : "false");
			}
			else if (v.tag == Value::Tag::None)
			{
				std::cout << "None";
			}
			else if (v.tag == Value::Tag::HeapPtr && v.heap)
			{
				switch (v.heap->getType())
				{
				case VMValue::Type::String:
					std::cout << static_cast<VMString *>(v.heap)->value;
					break;

				case VMValue::Type::Function:
				case VMValue::Type::Closure:
					std::cout << "FUNCTION";
					break;

				case VMValue::Type::Reference:
					throw std::runtime_error("RuntimeException: Cannot print reference value");
					break;

				case VMValue::Type::Record:
				{
					const VMRecord *record = static_cast<const VMRecord *>(v.heap);
					std::vector<std::string> keys;
					keys.reserve(record->fields.size());
					for (const auto &pair : record->fields)
					{
						keys.push_back(pair.first);
					}
					std::sort(keys.begin(), keys.end());

					std::cout << "{";
					if (!keys.empty())
					{
						for (size_t i = 0; i < keys.size(); ++i)
						{
							if (i > 0)
								std::cout << " ";
							std::cout << keys[i] << ":";

							const Value &fv = record->fields.at(keys[i]);
							printValue(fv);
						}
						std::cout << " ";
					}
					std::cout << "}";
					break;
				}
				}
			}
			else
			{
				std::cout << "<value>";
			}
		};

		for (const auto &arg : args)
		{
			printValue(arg);
		}

		std::cout << std::endl;
		return Value::none();
	}

	Value VirtualMachine::nativeInput(const std::vector<Value> &)
	{
		std::string line;
		std::getline(std::cin, line);
		VMString *s = heap_.allocate<VMString>(line);
		return Value::heapPtr(s);
	}

	Value VirtualMachine::nativeIntcast(const std::vector<Value> &args)
	{
		if (args.empty())
		{
			throw std::runtime_error("RuntimeException: intcast requires an argument");
		}

		const Value &arg = args[0];

		if (arg.tag == Value::Tag::HeapPtr && arg.heap &&
			arg.heap->getType() == VMValue::Type::String)
		{
			const std::string &s = static_cast<VMString *>(arg.heap)->value;
			try
			{
				int32_t v = std::stoi(s);
				return Value::integer(v);
			}
			catch (...)
			{
				throw std::runtime_error("IllegalCastException: Cannot cast string to integer");
			}
		}

		throw std::runtime_error("IllegalCastException: intcast requires string argument");
	}

} // namespace bytecode
