#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "./instructions.hpp"

namespace bytecode {
struct Constant {
  virtual ~Constant() {}

  struct None;
  struct Integer;
  struct String;
  struct Boolean;
};

struct Constant::None : public Constant {
  virtual ~None() {}
};

struct Constant::Integer : public Constant {
  Integer(int32_t value) : value(value) {}

  int32_t value;

  virtual ~Integer() {}
};

struct Constant::String : public Constant {
  String(std::string value) : value(value) {}

  std::string value;

  virtual ~String() {}
};

struct Constant::Boolean : public Constant {
  Boolean(bool value) : value(value) {}

  bool value;

  virtual ~Boolean() {}
};

struct Function {
  // List of functions defined within this function (but not functions defined
  // inside of nested functions)
  std::vector<Function *> functions_;

  // List of constants used by the instructions within this function (but not
  // nested functions)
  std::vector<Constant *> constants_;

  // The number of parameters to the function
  uint32_t parameter_count_;

  // List of local variables
  // The first parameter_count_ variables are the function's parameters
  // in their order as given in the paraemter list
  std::vector<std::string> local_vars_;

  // List of local variables accessed by reference (LocalReference)
  std::vector<std::string> local_reference_vars_;

  // List of the names of non-global and non-local variables accessed by the
  // function
  std::vector<std::string> free_vars_;

  // List of global variable and field names used inside the function
  std::vector<std::string> names_;

  std::vector<int32_t> local_ref_index_;

  void compute_local_ref_index() {
	  local_ref_index_.assign(local_vars_.size(), -1);

	  // For each referenceable var (by name), find its local index once
	  for (size_t ref_i = 0; ref_i < local_reference_vars_.size(); ++ref_i) {
		  const std::string &ref_name = local_reference_vars_[ref_i];

		  for (size_t local_i = 0; local_i < local_vars_.size(); ++local_i) {
			  if (local_vars_[local_i] == ref_name) {
				  local_ref_index_[local_i] = static_cast<int32_t>(ref_i);
				  break;
			  }
		  }
	  }
  }

  InstructionList instructions;
};
}; // namespace bytecode
