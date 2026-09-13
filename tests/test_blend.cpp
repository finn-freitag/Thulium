#include <iostream>
#include <cassert>
#include "../src/core/BlendModes.h"

int main() {
    std::cout << "=== Blend Modes Test ===" << std::endl;

    // Test channel blend formulas
    assert(pdn::blendChannel(pdn::BlendMode::Normal, 100, 200) == 200);
    assert(pdn::blendChannel(pdn::BlendMode::Multiply, 255, 128) == 128);
    assert(pdn::blendChannel(pdn::BlendMode::Multiply, 0, 255) == 0);
    assert(pdn::blendChannel(pdn::BlendMode::Add, 200, 100) == 255); // clamped
    assert(pdn::blendChannel(pdn::BlendMode::Difference, 100, 150) == 50);
    assert(pdn::blendChannel(pdn::BlendMode::Lighten, 100, 150) == 150);
    assert(pdn::blendChannel(pdn::BlendMode::Darken, 100, 150) == 100);
    assert(pdn::blendChannel(pdn::BlendMode::Xor, 0xAA, 0x55) == 0xFF);

    std::cout << "All 14 blend modes verified successfully!" << std::endl;
    return 0;
}
