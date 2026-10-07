# Comprehensive Edge Case Test Suite

## Test Categories

### 1. Arithmetic Edge Cases
- Division by zero
- Integer overflow
- Negative numbers
- Large numbers

### 2. Stack Edge Cases  
- Deep recursion (stack overflow)
- Empty stack operations
- Stack underflow on various operations

### 3. Variable Edge Cases
- Uninitialized variables
- Global vs local shadowing
- Reference variables in various contexts
- Multiple levels of closure nesting

### 4. Memory/GC Edge Cases
- Very low memory (1MB)
- Allocation during GC
- Circular references
- Large object graphs

### 5. Function Call Edge Cases
- Wrong argument counts
- Recursive calls
- Mutual recursion
- Deeply nested calls

### 6. Record Edge Cases
- Empty records
- Large records
- Nested records
- Record field access edge cases

### 7. Type Edge Cases
- Type mismatches in operations
- String concatenation edge cases
- Boolean operations
- None value handling

### 8. Control Flow Edge Cases
- Deeply nested conditionals
- Loops with early returns
- Complex boolean expressions

### 9. Closure Edge Cases
- Multiple captures
- Capturing in loops
- Closure returning closure
- Parameter capture

### 10. Native Function Edge Cases
- print with 0 args
- print with many args
- input edge cases
- intcast with invalid input
