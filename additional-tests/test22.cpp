#include "gc.hpp"
#include "objects.hpp"

#include <vector>
#include <iostream>
#include <string>

size_t Object::allocated = 0;
size_t Object::deallocated = 0;

// Test 22: Deep nested object graph with long chains
// Tests that deep pointer chains are followed correctly during marking
int main() {
    CollectedHeap heap;
    
    // Create a deep chain of 100 objects: root -> n1 -> n2 -> ... -> n99
    Integer* root = heap.allocate<Integer>(0);
    Integer* current = root;
    for (int i = 1; i < 100; ++i) {
        current->rhs = heap.allocate<Integer>(i);
        current = (Integer*)current->rhs;
    }
    
    // Create a tree branching off from position 50
    Integer* branch_point = root;
    for (int i = 0; i < 50; ++i) {
        branch_point = (Integer*)branch_point->rhs;
    }
    
    // Add left branch with 20 nodes
    Integer* left_branch = heap.allocate<Integer>(1000);
    branch_point->lhs = left_branch;
    for (int i = 1; i < 20; ++i) {
        left_branch->rhs = heap.allocate<Integer>(1000 + i);
        left_branch = (Integer*)left_branch->rhs;
    }
    
    // Total: 100 (main chain) + 20 (left branch) = 120 objects
    
    // Create many unreachable objects
    for (int i = 0; i < 200; ++i) {
        heap.allocate<Integer>(2000 + i);
    }
    
    if (Object::getAlive() != 320) {
        std::cerr << "Expected 320 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Run GC with only root in rootset
    std::vector<Object*> rootset;
    rootset.push_back(root);
    heap.gc(rootset.begin(), rootset.end());
    
    // Should keep 120 objects (main chain + left branch)
    if (Object::getAlive() != 120) {
        std::cerr << "Expected 120 alive objects after GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Verify deep chain is intact
    current = root;
    for (int i = 0; i < 100; ++i) {
        if (current->val != i) {
            std::cerr << "Chain corrupted at position " << i << std::endl;
            return 1;
        }
        current = (Integer*)current->rhs;
    }
    
    // Verify branch point has left branch
    branch_point = root;
    for (int i = 0; i < 50; ++i) {
        branch_point = (Integer*)branch_point->rhs;
    }
    
    if (branch_point->lhs == nullptr) {
        std::cerr << "Left branch lost after GC" << std::endl;
        return 1;
    }
    
    // Verify left branch values
    left_branch = (Integer*)branch_point->lhs;
    for (int i = 0; i < 20; ++i) {
        if (left_branch->val != 1000 + i) {
            std::cerr << "Left branch corrupted at position " << i << std::endl;
            return 1;
        }
        left_branch = (Integer*)left_branch->rhs;
    }
    
    std::cout << "Test 22 passed: Deep nested object graph traversal works correctly" << std::endl;
    return 0;
}
