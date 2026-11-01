#include<cstdint>

int main() {
    // Volatile accumulator to prevent the compiler from optimizing away the loop
    volatile uint64_t global_checksum = 0;

    volatile uint64_t a = 0;
    volatile uint64_t b = 1;
    
    volatile uint64_t iterations = 1000; 

    for (uint64_t i = 0; i < iterations; ++i) {
        uint64_t next = a + b;
        a = b;
        b = next;
        
        // Prevent overflow divergence
        if (b > 0xFFFFFFFFFFFFFFFFULL / 2) {
            b = 1;
            a = 0;
        }
        
        global_checksum ^= next;
    }
    return 0;
}