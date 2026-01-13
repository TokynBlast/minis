#include <cstdint>
#include <cstddef>
#include <iostream>

#define uint256 _BitInt(256)

uint256 x = ((uint256)0xFFFFFFFFFFFFFFFFULL << 192) |
            ((uint256)0xFFFFFFFFFFFFFFFFULL << 128) |
            ((uint256)0xFFFFFFFFFFFFFFFFULL << 64) |
            (uint256)0xFFFFFFFFFFFFFFFFULL;

int main() {

    std::cout << x << "\n";
    return 0;
}
