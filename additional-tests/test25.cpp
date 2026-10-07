#include "gc.hpp"
#include "objects.hpp"

#include <vector>
#include <iostream>
#include <string>

size_t Object::allocated = 0;
size_t Object::deallocated = 0;

// Test 25: Edge cases - null pointers, empty rootset, mixed types
// Tests various edge cases and boundary conditions
int main() {
    CollectedHeap heap;
    
    // Test 1: Empty rootset should collect everything
    std::vector<Integer*> temp_objects;
    for (int i = 0; i < 20; ++i) {
        temp_objects.push_back(heap.allocate<Integer>(i));
    }
    
    if (Object::getAlive() != 20) {
        std::cerr << "Test 1: Expected 20 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    std::vector<Object*> emptyRootset;
    heap.gc(emptyRootset.begin(), emptyRootset.end());
    
    if (Object::getAlive() != 0) {
        std::cerr << "Test 1: Expected 0 alive objects after GC with empty rootset but got " 
                  << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Test 2: Objects with null pointers
    Integer* obj1 = heap.allocate<Integer>(10);
    Integer* obj2 = heap.allocate<Integer>(20);
    Integer* obj3 = heap.allocate<Integer>(30);
    
    obj1->lhs = obj2;  // obj1 -> obj2
    obj1->rhs = nullptr;  // null pointer
    obj2->lhs = nullptr;  // null pointer
    obj2->rhs = obj3;  // obj2 -> obj3
    obj3->lhs = nullptr;  // null pointer
    obj3->rhs = nullptr;  // null pointer
    
    if (Object::getAlive() != 3) {
        std::cerr << "Test 2: Expected 3 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    std::vector<Object*> rootset2;
    rootset2.push_back(obj1);
    heap.gc(rootset2.begin(), rootset2.end());
    
    // All 3 should be reachable through obj1
    if (Object::getAlive() != 3) {
        std::cerr << "Test 2: Expected 3 alive objects after GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Test 3: Mix of Integer and String objects
    std::vector<Object*> rootset3;
    for (int i = 0; i < 10; ++i) {
        rootset3.push_back(heap.allocate<Integer>(i * 10));
        rootset3.push_back(heap.allocate<String>("str_" + std::to_string(i)));
    }
    
    // Create some unreachable mixed objects
    for (int i = 0; i < 15; ++i) {
        heap.allocate<Integer>(i * 100);
        heap.allocate<String>("orphan_" + std::to_string(i));
    }
    
    if (Object::getAlive() != 53) {  // 3 from test 2 + 20 from rootset3 + 30 unreachable
        std::cerr << "Test 3: Expected 53 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    heap.gc(rootset3.begin(), rootset3.end());
    
    if (Object::getAlive() != 20) {
        std::cerr << "Test 3: Expected 20 alive objects after GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Verify mixed types are intact
    for (int i = 0; i < 10; ++i) {
        Integer* intObj = (Integer*)rootset3[i * 2];
        String* strObj = (String*)rootset3[i * 2 + 1];
        
        if (intObj->val != i * 10) {
            std::cerr << "Test 3: Integer object corrupted at position " << i << std::endl;
            return 1;
        }
        
        if (strObj->val != "str_" + std::to_string(i)) {
            std::cerr << "Test 3: String object corrupted at position " << i << std::endl;
            return 1;
        }
    }
    
    // Test 4: Single object in rootset
    Integer* single = heap.allocate<Integer>(999);
    heap.allocate<Integer>(1);  // unreachable
    heap.allocate<Integer>(2);  // unreachable
    
    std::vector<Object*> rootset4;
    rootset4.push_back(single);
    heap.gc(rootset4.begin(), rootset4.end());
    
    if (Object::getAlive() != 1) {
        std::cerr << "Test 4: Expected 1 alive object after GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    if (single->val != 999) {
        std::cerr << "Test 4: Single object corrupted" << std::endl;
        return 1;
    }
    
    std::cout << "Test 25 passed: All edge cases handled correctly" << std::endl;
    return 0;
}
