#include "gc.hpp"
#include "objects.hpp"

#include <vector>
#include <iostream>
#include <string>

size_t Object::allocated = 0;
size_t Object::deallocated = 0;

// Test 21: Large-scale selective garbage collection with Strings
// Tests GC with 1000+ objects including many String objects
int main() {
    CollectedHeap heap;
    
    // Allocate 500 integers and 500 strings (1000 total)
    std::vector<Integer*> integers;
    std::vector<String*> strings;
    for (int i = 0; i < 500; ++i) {
        integers.push_back(heap.allocate<Integer>(i));
        strings.push_back(heap.allocate<String>("value_" + std::to_string(i)));
    }
    
    if (Object::getAlive() != 1000) {
        std::cerr << "Expected 1000 alive objects but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Only keep every 5th integer and every 10th string in rootset (200 total)
    std::vector<Object*> rootset;
    for (int i = 0; i < 500; i += 5) {
        rootset.push_back(integers[i]);
    }
    for (int i = 0; i < 500; i += 10) {
        rootset.push_back(strings[i]);
    }
    
    // Run GC - should keep only 150 objects (100 integers + 50 strings)
    heap.gc(rootset.begin(), rootset.end());
    
    if (Object::getAlive() != 150) {
        std::cerr << "Expected 150 alive objects after GC but got " << Object::getAlive() << std::endl;
        return 1;
    }
    
    // Verify the kept objects still have correct values
    for (int i = 0; i < 100; ++i) {
        Integer* obj = (Integer*)rootset[i];
        if (obj->val != i * 5) {
            std::cerr << "Expected " << (i * 5) << " but got " << obj->val << std::endl;
            return 1;
        }
    }
    
    for (int i = 0; i < 50; ++i) {
        String* obj = (String*)rootset[100 + i];
        std::string expected = "value_" + std::to_string(i * 10);
        if (obj->val != expected) {
            std::cerr << "Expected " << expected << " but got " << obj->val << std::endl;
            return 1;
        }
    }
    
    std::cout << "Test 21 passed: Large-scale selective GC with Strings works correctly" << std::endl;
    return 0;
}
