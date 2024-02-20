#pragma once
#include <vector>
#include <cstdint>


namespace mgm {
    class MemoryBlock {
        std::vector<uint8_t> data;

        public:
        MemoryBlock() = default;
    };
}
