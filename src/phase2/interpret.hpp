#pragma once

#include <stack>
#include <vector>
#include <iostream>

#include "../phase1/ast.hpp"
#include "../phase1/parse.hpp"

namespace interpretcode {
    void interpret(std::string &file_contents, size_t memLimit);
}
