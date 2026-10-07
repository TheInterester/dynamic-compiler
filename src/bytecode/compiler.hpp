#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "./instructions.hpp"
#include "./types.hpp"
#include "./optimizer.hpp"
#include "../phase1/ast.hpp"

class BytecodeCompiler : public ast::Visitor
{
	struct AnalysisFrame
	{
		const ast::FunctionDecl *owner = nullptr;
		AnalysisFrame *parent = nullptr;

		// Raw
		std::unordered_set<std::string> globals;
		std::unordered_set<std::string> assigns;
		std::unordered_set<std::string> reads;

		// Derived
		std::unordered_set<std::string> locals;
		std::unordered_set<std::string> localsByRef;
		std::unordered_set<std::string> freeVars;

		// Ordered for stable indices
		std::vector<std::string> params;
		std::vector<std::string> localsOrdered;
		std::vector<std::string> localsByRefOrdered;
		std::vector<std::string> freeVarsOrdered;
	};

	// Map to have stored analysis frames for each function
	std::unordered_map<const ast::FunctionDecl *, AnalysisFrame> frames_;
	AnalysisFrame *moduleFrame_ = nullptr;

	std::unique_ptr<bytecode::Function> fn_;

	// Name -> index maps for current compiling function
	std::unordered_map<std::string, int> localIndex_;
	std::unordered_map<std::string, int> localRefIndex_;
	std::unordered_map<std::string, int> freeIndex_;

	std::vector<std::string> localsOrdered_;
	const std::unordered_set<std::string> *globalSet_ = nullptr;

	// integer, string, and name constants for current compiling function
	std::unordered_map<int32_t, int> iconst_;
	std::unordered_map<std::string, int> sconst_;
	std::unordered_map<std::string, int> nameIndex_;

	int none_idx_ = -1;
	int true_idx_ = -1;
	int false_idx_ = -1;

	// Optimization flags
	std::vector<std::string> optimizations_;

	// Add stubs for native functions to outermost function
	void installNativeStubs()
	{

		auto mk = [&](uint32_t params)
		{
			auto *f = new bytecode::Function();
			f->parameter_count_ = params;
			return f;
		};

		const int printIdx = static_cast<int>(fn_->functions_.size());
		fn_->functions_.push_back(mk(1));

		const int inputIdx = static_cast<int>(fn_->functions_.size());
		fn_->functions_.push_back(mk(0));

		const int intcastIdx = static_cast<int>(fn_->functions_.size());
		fn_->functions_.push_back(mk(1));

		auto bind = [&](int funcIdx, const std::string &name)
		{
			emit(bytecode::Operation::LoadFunc, funcIdx);
			emit(bytecode::Operation::AllocClosure, 0);
			emit(bytecode::Operation::StoreGlobal, nameIdx(name));
		};

		bind(printIdx, "print");
		bind(inputIdx, "input");
		bind(intcastIdx, "intcast");
	}

public:
	// Set optimization flags
	void setOptimizations(const std::vector<std::string> &opts)
	{
		optimizations_ = opts;
	}

	// Check if an optimization is enabled
	bool hasOptimization(const std::string &opt) const
	{
		for (const auto &o : optimizations_)
		{
			if (o == opt || o == "all")
				return true;
		}
		return false;
	}

	bytecode::Function *compile(ast::ParseNode &program)
	{
		analyzeModule(program);

		resetFunctionState();
		fn_ = std::make_unique<bytecode::Function>();
		beginFunction(nullptr);

		std::vector<std::string> globals(moduleFrame_->assigns.begin(), moduleFrame_->assigns.end());
		std::sort(globals.begin(), globals.end());
		for (auto &g : globals)
			(void)nameIdx(g);
		installNativeStubs();

		bool programLeaves = leavesValue(program);
		program.accept(*this);
		if (programLeaves)
			emit(bytecode::Operation::Pop);

		// Apply peephole optimization if enabled
		bytecode::Function *result = fn_.release();
		if (hasOptimization("peephole"))
		{
			bytecode::PeepholeOptimizer::optimize(result, false);
		}

		return result;
	}

private:
	// {{{ ANALYSIS

	// Takes in the root in order to Initialize AnalysisFrame generation
	void analyzeModule(ast::ParseNode &root)
	{
		moduleFrame_ = &frames_[nullptr];
		moduleFrame_->owner = nullptr;
		moduleFrame_->parent = nullptr;

		collectGlobalsAssignsReadsIn(*moduleFrame_, root);

		for (const auto &name : moduleFrame_->assigns)
		{
			moduleFrame_->globals.insert(name);
		}

		finalizeLocals(*moduleFrame_);
		analyzeChildrenOf(*moduleFrame_);

		propagateCapturesAll();
		orderAllFrames();
	}

	// Performs DFS through functions to generate AnalysisFrame for all functions
	void analyzeChildrenOf(AnalysisFrame &fr)
	{
		// 1. Snapshot all direct children of this frame FIRST,
		//    so we don't iterate the unordered_map while mutating it.
		std::vector<AnalysisFrame *> children;
		children.reserve(frames_.size());
		for (auto &kv : frames_)
		{
			auto &child = kv.second;
			if (child.parent == &fr && child.owner != nullptr)
			{
				children.push_back(&child);
			}
		}

		// 2. Now process each child. Any nested functions discovered while
		//    analyzing this child’s body will be added to frames_, but
		//    they’ll be picked up by the *recursive* call below, not this loop.
		for (AnalysisFrame *child : children)
		{
			if (child->owner && child->owner->body)
			{
				collectGlobalsAssignsReadsIn(*child, *child->owner->body);
			}

			child->params.clear();
			for (auto &p : child->owner->args)
			{
				child->params.push_back(p->name);
				child->assigns.insert(p->name);
			}

			finalizeLocals(*child);
			analyzeChildrenOf(*child);
		}
	}

	// Collects all global keywords and assignment operations to see used variables
	void collectGlobalsAssignsReadsIn(AnalysisFrame &fr, ast::ParseNode &node)
	{
		struct V : ast::Visitor
		{
			AnalysisFrame &fr;
			BytecodeCompiler &self;
			V(AnalysisFrame &fr, BytecodeCompiler &self) : fr(fr), self(self) {}

			void visit(ast::Global &g) override { fr.globals.insert(g.name); }

			void visit(ast::FunctionDecl &f) override
			{
				auto &child = self.frames_[&f];
				child.owner = &f;
				child.parent = &fr;
			}

			void visit(ast::Assignment &a) override
			{
				if (auto *id = dynamic_cast<ast::Identifier *>(a.target.get()))
				{
					fr.assigns.insert(id->name);
				}
				else
				{
					a.target->accept(*this);
				}
				a.value->accept(*this);
			}

			void visit(ast::Identifier &id) override
			{
				fr.reads.insert(id.name);
			}

			void visit(ast::Block &n) override
			{
				n.left->accept(*this);
				n.right->accept(*this);
			}
			void visit(ast::If &n) override
			{
				n.condition->accept(*this);
				n.then->accept(*this);
				if (n.else_)
					n.else_->accept(*this);
			}
			void visit(ast::While &n) override
			{
				n.condition->accept(*this);
				n.body->accept(*this);
			}
			void visit(ast::Return &n) override
			{
				if (n.expr)
					n.expr->accept(*this);
			}
			void visit(ast::Call &n) override
			{
				n.location->accept(*this);
				for (auto &a : n.args)
					a->accept(*this);
			}
			void visit(ast::Record &n) override
			{
				for (auto &kv : n.fields)
					kv.second->accept(*this);
			}
			void visit(ast::BinOp &n) override
			{
				n.lhs->accept(*this);
				n.rhs->accept(*this);
			}
			void visit(ast::FieldDereference &n) override { n.base->accept(*this); }
			void visit(ast::IndexExpression &n) override
			{
				n.base->accept(*this);
				n.index->accept(*this);
			}
			void visit(ast::UnOp &n) override { n.expr->accept(*this); }
			void visit(ast::Closure &n) override { n.body->accept(*this); }

			void visit(ast::Integer &) override {}
			void visit(ast::String &) override {}
			void visit(ast::None &) override {}
			void visit(ast::Boolean &) override {}
			void visit(ast::Nop &) override {}
		} v(fr, *this);
		node.accept(v);
	}

	// Only include variables in locals if they aren't a global variable
	static void finalizeLocals(AnalysisFrame &fr)
	{
		fr.locals.clear();
		for (const auto &x : fr.assigns)
			if (!fr.globals.count(x))
				fr.locals.insert(x);
	}

	// Propagates free variables up so we know which variables are accessible by reference and which variables are free variables
	void propagateCapturesAll()
	{
		bool changed = true;
		while (changed)
		{
			changed = false;
			for (auto &kv : frames_)
			{
				AnalysisFrame &fr = kv.second;

				for (const auto &name : fr.reads)
				{
					if (fr.locals.count(name) || fr.globals.count(name))
						continue;

					AnalysisFrame *p = fr.parent;
					AnalysisFrame *def = nullptr;
					while (p)
					{
						if (p->locals.count(name))
						{
							def = p;
							break;
						}
						if (p->globals.count(name))
						{
							def = nullptr;
							break;
						}
						p = p->parent;
					}
					if (!def)
						continue;

					if (def->localsByRef.insert(name).second)
						changed = true;

					AnalysisFrame *q = &fr;
					while (q && q != def)
					{
						if (q->freeVars.insert(name).second)
							changed = true;
						q = q->parent;
					}
				}
			}
		}
	}

	// Ensures deterministic ordering of variables by ordering alphabetically while making sure parameters stay in order of declaration
	void orderAllFrames()
	{
		auto order = [](const std::unordered_set<std::string> &S)
		{
			std::vector<std::string> v(S.begin(), S.end());
			std::sort(v.begin(), v.end());
			return v;
		};

		for (auto &kv : frames_)
		{
			AnalysisFrame &fr = kv.second;

			std::unordered_set<std::string> paramSet(fr.params.begin(), fr.params.end());

			std::vector<std::string> others;
			others.reserve(fr.locals.size());
			for (auto &x : fr.locals)
				if (!paramSet.count(x))
					others.push_back(x);
			std::sort(others.begin(), others.end());

			fr.localsOrdered.clear();
			fr.localsOrdered.insert(fr.localsOrdered.end(), fr.params.begin(), fr.params.end());
			fr.localsOrdered.insert(fr.localsOrdered.end(), others.begin(), others.end());

			fr.localsByRefOrdered = order(fr.localsByRef);
			fr.freeVarsOrdered = order(fr.freeVars);
		}
	}
	// }}}

	// {{{ CODEGEN
	void resetFunctionState()
	{
		localIndex_.clear();
		localRefIndex_.clear();
		freeIndex_.clear();
		localsOrdered_.clear();
		globalSet_ = nullptr;

		iconst_.clear();
		sconst_.clear();
		nameIndex_.clear();

		none_idx_ = -1;
		true_idx_ = -1;
		false_idx_ = -1;
	}

	void beginFunction(const ast::FunctionDecl *fnNode)
	{
		AnalysisFrame &fr = frames_.at(fnNode);

		fn_->parameter_count_ = fnNode ? (uint32_t)fr.params.size() : 0;

		fn_->local_vars_ = fr.localsOrdered;
		fn_->local_reference_vars_ = fr.localsByRefOrdered;
		fn_->free_vars_ = fr.freeVarsOrdered;

		fn_->compute_local_ref_index();

		localsOrdered_ = fr.localsOrdered;

		localIndex_.clear();
		for (int i = 0; i < static_cast<int>(fr.localsOrdered.size()); ++i)
			localIndex_[fr.localsOrdered[i]] = i;

		localRefIndex_.clear();
		for (int i = 0; i < static_cast<int>(fr.localsByRefOrdered.size()); ++i)
			localRefIndex_[fr.localsByRefOrdered[i]] = i;

		freeIndex_.clear();
		for (int i = 0; i < static_cast<int>(fr.freeVarsOrdered.size()); ++i)
			freeIndex_[fr.freeVarsOrdered[i]] = i;

		globalSet_ = &fr.globals;
	}

	//  Adds next instruction to function instruction set
	void emit(bytecode::Operation op, std::optional<int32_t> operand0 = std::nullopt)
	{
		fn_->instructions.emplace_back(op, operand0);
	}

	// Gets current instruction position
	int here() const { return static_cast<int>(fn_->instructions.size()); }

	// Instructions requiring jumps need placeholders to be filled in later in bytecode compilation
	int emitIfPlaceholder()
	{
		fn_->instructions.emplace_back(bytecode::Operation::If, std::nullopt);
		return here() - 1;
	}
	int emitGotoPlaceholder()
	{
		fn_->instructions.emplace_back(bytecode::Operation::Goto, std::nullopt);
		return here() - 1;
	}

	// Adds operand to emitted placeholder at atIdx with relative targetIdx location
	void patchRelative(int atIdx, int targetIdx)
	{
		int32_t rel = targetIdx - atIdx;
		fn_->instructions[atIdx].operand0 = rel;
	}

	// {{{ Constant helpers
	int kInt(int32_t v)
	{
		if (auto it = iconst_.find(v); it != iconst_.end())
			return it->second;
		int idx = static_cast<int>(fn_->constants_.size());
		fn_->constants_.push_back(new bytecode::Constant::Integer(v));
		iconst_[v] = idx;
		return idx;
	}
	int kString(const std::string &s)
	{
		if (auto it = sconst_.find(s); it != sconst_.end())
			return it->second;
		int idx = static_cast<int>(fn_->constants_.size());
		fn_->constants_.push_back(new bytecode::Constant::String(s));
		sconst_[s] = idx;
		return idx;
	}
	int kNone()
	{
		if (none_idx_ >= 0)
			return none_idx_;
		none_idx_ = static_cast<int>(fn_->constants_.size());
		fn_->constants_.push_back(new bytecode::Constant::None());
		return none_idx_;
	}
	int kBoolean(bool b)
	{
		int &slot = b ? true_idx_ : false_idx_;
		if (slot >= 0)
			return slot;
		slot = static_cast<int>(fn_->constants_.size());
		fn_->constants_.push_back(new bytecode::Constant::Boolean(b));
		return slot;
	}
	int nameIdx(const std::string &s)
	{
		auto it = nameIndex_.find(s);
		if (it != nameIndex_.end())
			return it->second;
		int idx = static_cast<int>(fn_->names_.size());
		fn_->names_.push_back(s);
		nameIndex_[s] = idx;
		return idx;
	}
	// }}}

	// {{{ Other Helpers
	bool lastIs(bytecode::Operation op) const
	{
		if (fn_->instructions.empty())
			return false;
		return fn_->instructions.back().operation == op;
	}

	// Tries to prevent double pops to see if we just consumed a value
	bool justConsumedValue() const
	{
		if (fn_->instructions.empty())
			return false;
		using O = bytecode::Operation;
		switch (fn_->instructions.back().operation)
		{
		case O::Pop:
		case O::Return:
		case O::StoreLocal:
		case O::StoreGlobal:
		case O::StoreReference:
		case O::FieldStore:
		case O::IndexStore:
		case O::Goto:
		case O::If:
			return true;
		default:
			return false;
		}
	}

	// Checks if value is left on stack to see if we need to add an additional pop operation
	bool leavesValue(ast::ParseNode &n)
	{
		struct V : ast::Visitor
		{
			bool result = false;

			// Statements (don’t leave a value)
			void visit(ast::Assignment &) override { result = false; }
			void visit(ast::Global &) override { result = false; }
			void visit(ast::Return &) override { result = false; }
			void visit(ast::If &) override { result = false; }
			void visit(ast::While &) override { result = false; }

			// Block leaves *whatever right leaves*
			void visit(ast::Block &b) override
			{
				b.right->accept(*this);
			}

			// Expressions (do leave a value)
			void visit(ast::Identifier &) override { result = true; }
			void visit(ast::Integer &) override { result = true; }
			void visit(ast::String &) override { result = true; }
			void visit(ast::None &) override { result = true; }
			void visit(ast::Boolean &) override { result = true; }
			void visit(ast::Call &) override { result = true; }
			void visit(ast::Record &) override { result = true; }
			void visit(ast::BinOp &) override { result = true; }
			void visit(ast::UnOp &) override { result = true; }
			void visit(ast::FieldDereference &) override { result = true; }
			void visit(ast::IndexExpression &) override { result = true; }
			void visit(ast::Closure &) override { result = true; }
			void visit(ast::FunctionDecl &) override { result = true; }

			void visit(ast::Nop &) override { result = false; }
		} v;
		n.accept(v);
		return v.result;
	}

	// Handles variable stores whether it's a global, local, or reference
	void handleStore(const std::string &name)
	{
		// Local variable that can be referenced
		if (auto rt = localRefIndex_.find(name); rt != localRefIndex_.end())
		{
			auto idx = localIndex_.find(name);
			assert(idx != localIndex_.end());
			emit(bytecode::Operation::StoreLocal, idx->second);
			return;
		}

		// Free variable inherited from parent function
		if (auto ft = freeIndex_.find(name); ft != freeIndex_.end())
		{
			int refIdx = (int)fn_->local_reference_vars_.size() + ft->second;
			emit(bytecode::Operation::PushReference, refIdx);
			emit(bytecode::Operation::Swap);
			emit(bytecode::Operation::StoreReference);
			return;
		}

		// Global variables
		if (globalSet_ && globalSet_->count(name))
		{
			emit(bytecode::Operation::StoreGlobal, nameIdx(name));
			return;
		}

		// Local variables
		if (auto it = localIndex_.find(name); it != localIndex_.end())
		{
			emit(bytecode::Operation::StoreLocal, it->second);
			return;
		}

		// Fallback
		emit(bytecode::Operation::StoreGlobal, nameIdx(name));
	}

	// Handles adding nested functions
	int compileNestedFunction(const ast::FunctionDecl &f)
	{

		// Save parent state
		auto saved_fn = std::move(fn_);
		auto saved_localIndex = std::move(localIndex_);
		auto saved_localRefIndex = std::move(localRefIndex_);
		auto saved_freeIndex = std::move(freeIndex_);
		auto saved_localsOrdered = std::move(localsOrdered_);
		auto saved_globalSet = globalSet_;
		auto saved_iconst = std::move(iconst_);
		auto saved_sconst = std::move(sconst_);
		auto saved_nameIndex = std::move(nameIndex_);
		int saved_none = none_idx_;
		int saved_true = true_idx_;
		int saved_false = false_idx_;

		// Clear function vars to be used for child function
		resetFunctionState();
		fn_ = std::make_unique<bytecode::Function>();
		beginFunction(&f);

		size_t paramCount = fn_->parameter_count_;
		for (size_t i = paramCount; i < fn_->local_vars_.size(); ++i)
		{
			emit(bytecode::Operation::LoadConst, kNone());
			emit(bytecode::Operation::StoreLocal, static_cast<int>(i));
		}
		size_t instructionsBefore = fn_->instructions.size();
		bool bodyLeaves = false;
		// Body
		if (f.body)
		{
			bodyLeaves = leavesValue(*f.body);
			f.body->accept(*this);
		}

		// Always add implicit return if last instruction isn't already a return
		bool actuallyEmitted = fn_->instructions.size() > instructionsBefore;
		if (!lastIs(bytecode::Operation::Return))
		{
			if (bodyLeaves && actuallyEmitted && !justConsumedValue())
				emit(bytecode::Operation::Pop);
		}
		emit(bytecode::Operation::LoadConst, kNone());
		emit(bytecode::Operation::Return);
		// Attach to parent function’s functions_ table
		bytecode::Function *childPtr = fn_.release();

		// Restore parent state
		fn_.reset(saved_fn.release());
		localIndex_ = std::move(saved_localIndex);
		localRefIndex_ = std::move(saved_localRefIndex);
		freeIndex_ = std::move(saved_freeIndex);
		localsOrdered_ = std::move(saved_localsOrdered);
		globalSet_ = saved_globalSet;
		iconst_ = std::move(saved_iconst);
		sconst_ = std::move(saved_sconst);
		nameIndex_ = std::move(saved_nameIndex);
		none_idx_ = saved_none;
		true_idx_ = saved_true;
		false_idx_ = saved_false;

		int idx = static_cast<int>(fn_->functions_.size());
		fn_->functions_.push_back(childPtr);
		return idx;
	}

	// Compute PushReference index for a variable name in the CURRENT function
	// Index space: [ local_reference_vars_ ... , free_vars_ ... ]
	int refIndexInCurrent(const std::string &name)
	{
		if (auto it = localRefIndex_.find(name); it != localRefIndex_.end())
			return it->second;

		// All referenceable variables are concatenated in one array
		if (auto it = freeIndex_.find(name); it != freeIndex_.end())
			return static_cast<int>(fn_->local_reference_vars_.size()) + it->second;

		// add as new free var
		int base = static_cast<int>(fn_->local_reference_vars_.size());
		int pos = static_cast<int>(fn_->free_vars_.size());
		fn_->free_vars_.push_back(name);
		freeIndex_[name] = pos;
		return base + pos;
	}
	// }}}

	// {{{ Visitors
	// Literals / names
	void visit(ast::Integer &n) override { emit(bytecode::Operation::LoadConst, kInt(n.value)); }
	void visit(ast::String &n) override { emit(bytecode::Operation::LoadConst, kString(n.value)); }
	void visit(ast::None &) override { emit(bytecode::Operation::LoadConst, kNone()); }
	void visit(ast::Boolean &n) override { emit(bytecode::Operation::LoadConst, kBoolean(n.value)); }

	void visit(ast::Identifier &n) override
	{
		// Check global first - if declared global, always use global
		if (globalSet_ && globalSet_->count(n.name))
		{
			emit(bytecode::Operation::LoadGlobal, nameIdx(n.name));
			return;
		}

		// local var
		if (auto it = localIndex_.find(n.name); it != localIndex_.end())
		{
			emit(bytecode::Operation::LoadLocal, it->second);
			return;
		}

		// free var
		if (auto ft = freeIndex_.find(n.name); ft != freeIndex_.end())
		{
			int refIdx = static_cast<int>(fn_->local_reference_vars_.size()) + ft->second;
			emit(bytecode::Operation::PushReference, refIdx);
			emit(bytecode::Operation::LoadReference);
			return;
		}

		// fallback to global var (undeclared)
		emit(bytecode::Operation::LoadGlobal, nameIdx(n.name));
	}

	// This should be handled during analysis phase
	void visit(ast::Global &) override {}

	// Field / Index expressions
	void visit(ast::FieldDereference &n) override
	{
		n.base->accept(*this);
		emit(bytecode::Operation::FieldLoad, nameIdx(n.field->name));
	}

	void visit(ast::IndexExpression &n) override
	{
		n.base->accept(*this);
		n.index->accept(*this);
		emit(bytecode::Operation::IndexLoad);
	}

	// Unary / Binary Operations
	void visit(ast::UnOp &n) override
	{
		n.expr->accept(*this);
		switch (n.op)
		{
		case ast::UnaryOp::NOT:
			emit(bytecode::Operation::Not);
			break;
		case ast::UnaryOp::NEG:
			emit(bytecode::Operation::Neg);
			break;
		}
	}
	void visit(ast::BinOp& n) override {
		//constant folding
		if (n.intFoldable) {
			struct V : ast::Visitor {
				int rIntVal_;
				bool rBoolVal_;
				std::string rStringVal_;

				void visit(ast::Identifier &identifier) override {};
				void visit(ast::Block &block) override {};
				void visit(ast::Closure &closure) override {};
				void visit(ast::Global &global) override {};
				void visit(ast::If &if_) override {};
				void visit(ast::While &while_) override {};
				void visit(ast::Return &return_) override {};
				void visit(ast::Assignment &assignment) override {};
				void visit(ast::Call &call) override {};
				void visit(ast::FunctionDecl &function_decl) override {};
				void visit(ast::Record &record) override {};
				
				void visit(ast::FieldDereference &field_deref) override {};
				void visit(ast::IndexExpression &idx_expr) override {};
				void visit(ast::UnOp &unop) override { 
					unop.expr->accept(*this);
					if (unop.op == ast::UnaryOp::NEG) {
						rIntVal_ *= -1;
					} else {
						//unop is Not
						rBoolVal_ = !rBoolVal_;
					}
				};
				void visit(ast::Integer &integer) override {rIntVal_ = integer.value; };
				void visit(ast::String &string) override { rStringVal_ = string.value; };
				void visit(ast::None &none) override {};
				void visit(ast::Boolean &boolean) override { rBoolVal_ = boolean.value; };
				void visit(ast::Nop &nop) override {};

				void visit(ast::BinOp &binop) override {
					binop.lhs->accept(*this);
					int lVal = rIntVal_;
					binop.rhs->accept(*this);
					int rVal = rIntVal_;
					using BO = ast::BinaryOp;
					switch (binop.op) {
						case BO::ADD: rIntVal_ = lVal + rVal; break;
						case BO::SUB: rIntVal_ = lVal - rVal; break;
						case BO::MUL: rIntVal_ = lVal * rVal; break;
						case BO::DIV: rIntVal_ = lVal / rVal; break;
					}
				};

			} v;
			
			n.accept(v);
			emit(bytecode::Operation::LoadConst, kInt(v.rIntVal_));
			return;
		} 
		n.lhs->accept(*this);
		n.rhs->accept(*this);
		using BO = ast::BinaryOp;
		switch (n.op)
		{
		case BO::ADD:
			emit(bytecode::Operation::Add);
			break;
		case BO::SUB:
			emit(bytecode::Operation::Sub);
			break;
		case BO::MUL:
			emit(bytecode::Operation::Mul);
			break;
		case BO::DIV:
			emit(bytecode::Operation::Div);
			break;
		case BO::GT:
			emit(bytecode::Operation::Gt);
			break;
		case BO::GE:
			emit(bytecode::Operation::Geq);
			break;
		case BO::EQ:
			emit(bytecode::Operation::Eq);
			break;
		case BO::LT:
			emit(bytecode::Operation::Swap);
			emit(bytecode::Operation::Gt);
			break;
		case BO::LE:
			emit(bytecode::Operation::Swap);
			emit(bytecode::Operation::Geq);
			break;
		case BO::AND:
			emit(bytecode::Operation::And);
			break;
		case BO::OR:
			emit(bytecode::Operation::Or);
			break;
		}
	}

	// Records
	void visit(ast::Record &n) override
	{
		emit(bytecode::Operation::AllocRecord);
		for (auto &[key, expr] : n.fields)
		{
			emit(bytecode::Operation::Dup);
			expr->accept(*this);
			emit(bytecode::Operation::FieldStore, nameIdx(key));
		}
	}

	// Calls
	void visit(ast::Call &n) override
	{
		n.location->accept(*this);
		for (auto &a : n.args)
			a->accept(*this);
		emit(bytecode::Operation::Call, static_cast<int32_t>(n.args.size()));
	}

	// Assignment
	void visit(ast::Assignment &a) override
	{
		if (auto *id = dynamic_cast<ast::Identifier *>(a.target.get()))
		{
			a.value->accept(*this);
			handleStore(id->name);
			return;
		}
		if (auto *fd = dynamic_cast<ast::FieldDereference *>(a.target.get()))
		{
			fd->base->accept(*this);
			a.value->accept(*this);
			emit(bytecode::Operation::FieldStore, nameIdx(fd->field->name));
			return;
		}
		if (auto *ix = dynamic_cast<ast::IndexExpression *>(a.target.get()))
		{
			ix->base->accept(*this);
			ix->index->accept(*this);
			a.value->accept(*this);
			emit(bytecode::Operation::IndexStore);
			return;
		}

		a.value->accept(*this);
		emit(bytecode::Operation::Pop);
	}

	void visit(ast::Block &n) override
	{
		bool leftLeaves = leavesValue(*n.left);
		n.left->accept(*this);
		bool rightIsNop = dynamic_cast<ast::Nop *>(n.right.get()) != nullptr;
		if (leftLeaves && !rightIsNop && !justConsumedValue())
			emit(bytecode::Operation::Pop);
		n.right->accept(*this);
	}

	void visit(ast::If &n) override
	{
		n.condition->accept(*this);
		emit(bytecode::Operation::Not);
		int ifIdx = emitIfPlaceholder();

		bool thenLeaves = leavesValue(*n.then);
		n.then->accept(*this);
		if (thenLeaves && !justConsumedValue())
			emit(bytecode::Operation::Pop);

		// Use to avoid goto 1 in bytecode when else is a NOP
		bool hasRealElse = n.else_ && dynamic_cast<ast::Nop *>(n.else_.get()) == nullptr;

		if (hasRealElse)
		{
			// Jump past else
			int gotoIdx = emitGotoPlaceholder();
			int elseIdx = here();
			patchRelative(ifIdx, elseIdx);

			bool elseLeaves = leavesValue(*n.else_);
			n.else_->accept(*this);
			if (elseLeaves && !justConsumedValue())
				emit(bytecode::Operation::Pop);

			int endIdx = here();
			patchRelative(gotoIdx, endIdx);
		}
		else
		{
			int endIdx = here();
			patchRelative(ifIdx, endIdx);
		}
	}

	void visit(ast::While &n) override
	{
		int head = here();

		n.condition->accept(*this);
		emit(bytecode::Operation::Not);
		int ifIdx = emitIfPlaceholder();

		bool bodyLeaves = leavesValue(*n.body);
		n.body->accept(*this);
		if (bodyLeaves && !justConsumedValue())
			emit(bytecode::Operation::Pop);

		int back = emitGotoPlaceholder();
		patchRelative(back, head);

		int end = here();
		patchRelative(ifIdx, end);
	}

	void visit(ast::Return &n) override
	{
		if (n.expr)
		{
			n.expr->accept(*this);
		}
		else
		{
			emit(bytecode::Operation::LoadConst, kNone());
		}
		emit(bytecode::Operation::Return);
	}

	void visit(ast::Closure &n) override
	{
		n.body->accept(*this);
	}

	void visit(ast::FunctionDecl &f) override
	{
		int childIdx = compileNestedFunction(f);

		emit(bytecode::Operation::LoadFunc, childIdx);

		const auto &childFrame = frames_.at(&f);
		for (const auto &freeName : childFrame.freeVarsOrdered)
		{
			int refIdx = refIndexInCurrent(freeName);
			emit(bytecode::Operation::PushReference, refIdx);
		}

		emit(bytecode::Operation::AllocClosure, static_cast<int32_t>(childFrame.freeVarsOrdered.size()));
	}

	// Nop 😢
	void visit(ast::Nop &) override {} // }}}
									   // }}}
};
