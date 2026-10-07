#include "optimizer.hpp"
#include <iostream>
#include <algorithm>

namespace bytecode
{

    int PeepholeOptimizer::optimize(Function *fn, bool verbose)
    {
        if (!fn)
            return 0;

        int totalEliminated = 0;

        // Optimize this function's instructions
        totalEliminated += optimizeInstructions(fn->instructions, fn->constants_);

        // Recursively optimize nested functions
        for (auto *nestedFn : fn->functions_)
        {
            totalEliminated += optimize(nestedFn, verbose);
        }

        if (verbose && totalEliminated > 0)
        {
            std::cerr << "Peephole: eliminated " << totalEliminated << " instructions" << std::endl;
        }

        return totalEliminated;
    }

    int PeepholeOptimizer::optimizeInstructions(InstructionList &instrs, const std::vector<Constant *> &constants)
    {
        int eliminated = 0;
        bool changed = true;

        // Keep iterating until no more optimizations can be applied
        while (changed)
        {
            changed = false;

            for (size_t i = 0; i < instrs.size();)
            {
                bool optimized = false;
                int deleteCount = 0;

                // Try each pattern in order
                if (matchLoadPop(instrs, i))
                {
                    // Delete both instructions
                    instrs.erase(instrs.begin() + i, instrs.begin() + i + 2);
                    deleteCount = 2;
                    optimized = true;
                }
                else if (matchDupPop(instrs, i))
                {
                    // Delete both instructions
                    instrs.erase(instrs.begin() + i, instrs.begin() + i + 2);
                    deleteCount = 2;
                    optimized = true;
                }
                else if (matchDoubleNot(instrs, i))
                {
                    // Delete both Not instructions
                    instrs.erase(instrs.begin() + i, instrs.begin() + i + 2);
                    deleteCount = 2;
                    optimized = true;
                }
                else if (matchDoubleSwap(instrs, i))
                {
                    // Delete both Swap instructions
                    instrs.erase(instrs.begin() + i, instrs.begin() + i + 2);
                    deleteCount = 2;
                    optimized = true;
                }
                else if (matchGotoNext(instrs, i))
                {
                    // Delete the Goto instruction
                    instrs.erase(instrs.begin() + i);
                    deleteCount = 1;
                    optimized = true;
                }
                else if (matchAddZero(instrs, i, constants))
                {
                    // Delete LoadConst 0 and Add, keep first operand
                    instrs.erase(instrs.begin() + i, instrs.begin() + i + 2);
                    deleteCount = 2;
                    optimized = true;
                }
                else if (matchMulOne(instrs, i, constants))
                {
                    // Delete LoadConst 1 and Mul, keep first operand
                    instrs.erase(instrs.begin() + i, instrs.begin() + i + 2);
                    deleteCount = 2;
                    optimized = true;
                }
                else if (matchMulZero(instrs, i, constants))
                {
                    // Replace LoadConst X; Mul with just LoadConst 0
                    // Keep the LoadConst at position i, delete the Mul
                    instrs.erase(instrs.begin() + i + 1);
                    deleteCount = 1;
                    optimized = true;
                }

                if (optimized)
                {
                    // Adjust jump offsets for the deleted instructions
                    adjustJumps(instrs, i, deleteCount);
                    eliminated += deleteCount;
                    changed = true;
                    // Don't increment i - check same position again
                }
                else
                {
                    i++;
                }
            }
        }

        return eliminated;
    }

    bool PeepholeOptimizer::matchLoadPop(const InstructionList &instrs, size_t i)
    {
        if (i + 1 >= instrs.size())
            return false;

        return isLoadOp(instrs[i].operation) &&
               instrs[i + 1].operation == Operation::Pop;
    }

    bool PeepholeOptimizer::matchDupPop(const InstructionList &instrs, size_t i)
    {
        if (i + 1 >= instrs.size())
            return false;

        return instrs[i].operation == Operation::Dup &&
               instrs[i + 1].operation == Operation::Pop;
    }

    bool PeepholeOptimizer::matchDoubleNot(const InstructionList &instrs, size_t i)
    {
        if (i + 1 >= instrs.size())
            return false;

        return instrs[i].operation == Operation::Not &&
               instrs[i + 1].operation == Operation::Not;
    }

    bool PeepholeOptimizer::matchDoubleSwap(const InstructionList &instrs, size_t i)
    {
        if (i + 1 >= instrs.size())
            return false;

        return instrs[i].operation == Operation::Swap &&
               instrs[i + 1].operation == Operation::Swap;
    }

    bool PeepholeOptimizer::matchGotoNext(const InstructionList &instrs, size_t i)
    {
        if (i >= instrs.size())
            return false;

        return instrs[i].operation == Operation::Goto &&
               instrs[i].operand0.has_value() &&
               instrs[i].operand0.value() == 1;
    }

    bool PeepholeOptimizer::matchAddZero(const InstructionList &instrs, size_t i, const std::vector<Constant *> &constants)
    {
        if (i + 1 >= instrs.size())
            return false;

        if (instrs[i].operation != Operation::LoadConst || !instrs[i].operand0.has_value())
            return false;

        if (instrs[i + 1].operation != Operation::Add)
            return false;

        int32_t value;
        return getConstInt(constants, instrs[i].operand0.value(), value) && value == 0;
    }

    bool PeepholeOptimizer::matchMulOne(const InstructionList &instrs, size_t i, const std::vector<Constant *> &constants)
    {
        if (i + 1 >= instrs.size())
            return false;

        if (instrs[i].operation != Operation::LoadConst || !instrs[i].operand0.has_value())
            return false;

        if (instrs[i + 1].operation != Operation::Mul)
            return false;

        int32_t value;
        return getConstInt(constants, instrs[i].operand0.value(), value) && value == 1;
    }

    bool PeepholeOptimizer::matchMulZero(const InstructionList &instrs, size_t i, const std::vector<Constant *> &constants)
    {
        if (i + 1 >= instrs.size())
            return false;

        if (instrs[i].operation != Operation::LoadConst || !instrs[i].operand0.has_value())
            return false;

        if (instrs[i + 1].operation != Operation::Mul)
            return false;

        int32_t value;
        return getConstInt(constants, instrs[i].operand0.value(), value) && value == 0;
    }

    bool PeepholeOptimizer::isLoadOp(Operation op)
    {
        return op == Operation::LoadConst ||
               op == Operation::LoadLocal ||
               op == Operation::LoadGlobal ||
               op == Operation::LoadFunc;
    }

    void PeepholeOptimizer::adjustJumps(InstructionList &instrs, size_t deletePos, int deleteCount)
    {
        // Iterate through all instructions and adjust jump offsets
        for (size_t i = 0; i < instrs.size(); i++)
        {
            auto &instr = instrs[i];

            // Only process jump instructions (Goto and If)
            if ((instr.operation == Operation::Goto || instr.operation == Operation::If) &&
                instr.operand0.has_value())
            {
                int32_t offset = instr.operand0.value();
                int32_t targetIdx = static_cast<int32_t>(i) + offset;

                // If the jump crosses the deleted region, adjust it
                if (i < deletePos && targetIdx > static_cast<int32_t>(deletePos))
                {
                    // Jump forward crosses deletion point
                    instr.operand0 = offset - deleteCount;
                }
                else if (i >= deletePos && targetIdx < static_cast<int32_t>(deletePos))
                {
                    // Jump backward crosses deletion point
                    instr.operand0 = offset + deleteCount;
                }
                else if (i >= deletePos)
                {
                    // Both jump source and target are after deletion
                    // No adjustment needed to offset, but we've already moved in the array
                }
            }
        }
    }

    bool PeepholeOptimizer::getConstInt(const std::vector<Constant *> &constants, int32_t constIdx, int32_t &value)
    {
        if (constIdx < 0 || constIdx >= static_cast<int32_t>(constants.size()))
            return false;

        auto *intConst = dynamic_cast<Constant::Integer *>(constants[constIdx]);
        if (!intConst)
            return false;

        value = intConst->value;
        return true;
    }

} // namespace bytecode
