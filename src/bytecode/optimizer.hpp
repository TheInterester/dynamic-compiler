#pragma once

#include <vector>
#include <string>
#include "instructions.hpp"
#include "types.hpp"

namespace bytecode
{

    /**
     * PeepholeOptimizer - Performs local bytecode optimizations
     *
     * Identifies and eliminates redundant instruction patterns such as:
     * - Load-Pop pairs
     * - Dup-Pop pairs
     * - Double negations (Not-Not)
     * - Double swaps (Swap-Swap)
     * - Goto to next instruction
     * - Redundant constant operations (add 0, mul 1, etc.)
     */
    class PeepholeOptimizer
    {
    public:
        /**
         * Optimizes a function and all its nested functions recursively
         * @param fn The function to optimize
         * @param verbose If true, prints optimization statistics
         * @return Number of instructions eliminated
         */
        static int optimize(Function *fn, bool verbose = false);

    private:
        /**
         * Optimizes a single instruction list
         * @param instrs The instruction list to optimize
         * @param constants The constants table (for constant folding checks)
         * @return Number of instructions eliminated
         */
        static int optimizeInstructions(InstructionList &instrs, const std::vector<Constant *> &constants);

        /**
         * Pattern matching functions - return true if pattern matches at position i
         */
        static bool matchLoadPop(const InstructionList &instrs, size_t i);
        static bool matchDupPop(const InstructionList &instrs, size_t i);
        static bool matchDoubleNot(const InstructionList &instrs, size_t i);
        static bool matchDoubleSwap(const InstructionList &instrs, size_t i);
        static bool matchGotoNext(const InstructionList &instrs, size_t i);
        static bool matchAddZero(const InstructionList &instrs, size_t i, const std::vector<Constant *> &constants);
        static bool matchMulOne(const InstructionList &instrs, size_t i, const std::vector<Constant *> &constants);
        static bool matchMulZero(const InstructionList &instrs, size_t i, const std::vector<Constant *> &constants);

        /**
         * Checks if an instruction is a Load operation (LoadConst, LoadLocal, LoadGlobal)
         */
        static bool isLoadOp(Operation op);

        /**
         * Adjusts jump offsets after instructions are removed
         * @param instrs The instruction list
         * @param deletePos Position where instructions were deleted
         * @param deleteCount Number of instructions deleted
         */
        static void adjustJumps(InstructionList &instrs, size_t deletePos, int deleteCount);

        /**
         * Get integer value from constant if it's an integer constant
         */
        static bool getConstInt(const std::vector<Constant *> &constants, int32_t constIdx, int32_t &value);
    };

} // namespace bytecode
