#include "gc.hpp"
#include "objects.hpp"

#include <vector>
#include <iostream>
#include <string>

size_t Object::allocated = 0;
size_t Object::deallocated = 0;

// Test 23: Circular references
// Tests that GC can handle cycles in object graphs
int main() {
    CollectedHeap heap;
    
    // Create a circular linked list: obj1 -> obj2 -> obj3 -> obj1
    Integer* obj1 = heap.allocate<Integer>(1);
    Integer* obj2 = heap.allocate<Integer>(2);
    Integer* obj3 = heap.allocate<Integer>(3);
    
    obj1->rhs = obj2;
    obj2->rhs = obj3;
    obj3->rhs = obj1;  // Creates cycle
    
    // Create another cycle that's unreachable
    Integer* orphan1 = heap.allocate<Integer>(10);
    Integer* orphan2 = heap.allocate<Integer>(20);
    Integer* orphan3 = heap.allocate<Integer>(30);
    
    orphan1->rhs = orphan2;
    orphan2->rhs = orphan3;
    orphan3->rhs = orphan1;  // Creates cycle
    
    if (Object::getAlive() != 6) {
        std::cerr << "Expected 6 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Run GC with only obj1 in rootset
    std::vector<Object*> rootset;
    rootset.push_back(obj1);
    heap.gc(rootset.begin(), rootset.end());
    
    // Should keep 3 objects (obj1, obj2, obj3) and collect the orphan cycle
    if (Object::getAlive() != 3) {
        std::cerr << "Expected 3 alive objects after GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Verify the reachable cycle is intact
    if (obj1->val != 1 || ((Integer*)obj1->rhs)->val != 2) {
        std::cerr << "First cycle corrupted" << std::endl;
        return 1;
    }
    
    if (((Integer*)obj2->rhs)->val != 3 || ((Integer*)obj3->rhs)->val != 1) {
        std::cerr << "Circular reference broken" << std::endl;
        return 1;
    }
    
    // Run GC again with empty rootset - should collect everything
    std::vector<Object*> emptyRootset;
    heap.gc(emptyRootset.begin(), emptyRootset.end());
    
    if (Object::getAlive() != 0) {
        std::cerr << "Expected 0 alive objects after second GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    std::cout << "Test 23 passed: Circular references handled correctly" << std::endl;
    return 0;
}
