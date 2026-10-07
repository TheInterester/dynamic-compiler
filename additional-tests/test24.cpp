#include "gc.hpp"
#include "objects.hpp"

#include <vector>
#include <iostream>
#include <string>

size_t Object::allocated = 0;
size_t Object::deallocated = 0;

// Test 24: Multiple GC cycles and mark flag reset verification
// Tests that mark flags are properly reset between GC cycles
int main() {
    CollectedHeap heap;
    
    // Phase 1: Allocate initial objects
    std::vector<Integer*> phase1_objects;
    for (int i = 0; i < 50; ++i) {
        phase1_objects.push_back(heap.allocate<Integer>(i));
    }
    
    if (Object::getAlive() != 50) {
        std::cerr << "Phase 1: Expected 50 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // First GC: Keep first 10 objects
    std::vector<Object*> rootset1;
    for (int i = 0; i < 10; ++i) {
        rootset1.push_back(phase1_objects[i]);
    }
    heap.gc(rootset1.begin(), rootset1.end());
    
    if (Object::getAlive() != 10) {
        std::cerr << "After GC 1: Expected 10 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Phase 2: Allocate more objects
    std::vector<Integer*> phase2_objects;
    for (int i = 100; i < 150; ++i) {
        phase2_objects.push_back(heap.allocate<Integer>(i));
    }
    
    if (Object::getAlive() != 60) {
        std::cerr << "Phase 2: Expected 60 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Second GC: Keep only phase 2 objects (tests mark flag reset)
    std::vector<Object*> rootset2;
    for (int i = 0; i < 50; ++i) {
        rootset2.push_back(phase2_objects[i]);
    }
    heap.gc(rootset2.begin(), rootset2.end());
    
    if (Object::getAlive() != 50) {
        std::cerr << "After GC 2: Expected 50 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Verify phase2 objects are intact
    for (int i = 0; i < 50; ++i) {
        if (phase2_objects[i]->val != 100 + i) {
            std::cerr << "Phase 2 object corrupted: expected " << (100 + i) 
                      << " but got " << phase2_objects[i]->val << std::endl;
            return 1;
        }
    }
    
    // Phase 3: Create linked structures
    Integer* chain = phase2_objects[0];
    for (int i = 1; i < 25; ++i) {
        phase2_objects[i-1]->rhs = phase2_objects[i];
    }
    
    // Third GC: Keep only the chain head, rest should be reachable through pointers
    std::vector<Object*> rootset3;
    rootset3.push_back(chain);
    heap.gc(rootset3.begin(), rootset3.end());
    
    if (Object::getAlive() != 25) {
        std::cerr << "After GC 3: Expected 25 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Verify chain is intact
    Integer* current = chain;
    for (int i = 0; i < 25; ++i) {
        if (current->val != 100 + i) {
            std::cerr << "Chain corrupted at position " << i << std::endl;
            return 1;
        }
        current = (Integer*)current->rhs;
    }
    
    // Phase 4: Test mark flag reset - run GC multiple times with same rootset
    // This tests that marks are properly cleared between cycles
    for (int cycle = 0; cycle < 5; ++cycle) {
        heap.gc(rootset3.begin(), rootset3.end());
        
        if (Object::getAlive() != 25) {
            std::cerr << "After GC cycle " << (cycle + 4) << ": Expected 25 alive objects but got " 
                      << Object::getAlive() << std::endl;
            return 1;
        }
        
        // Verify chain is still intact
        current = chain;
        for (int i = 0; i < 25; ++i) {
            if (current->val != 100 + i) {
                std::cerr << "Chain corrupted at cycle " << cycle << ", position " << i << std::endl;
                return 1;
            }
            current = (Integer*)current->rhs;
        }
    }
    
    std::cout << "Test 24 passed: Multiple GC cycles and mark flag reset work correctly" << std::endl;
    return 0;
}
