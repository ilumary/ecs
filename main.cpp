#include <iostream>

#include "registry.hpp"

struct s1 {
    uint32_t i1;
    uint64_t i2;
};

struct s2 {
    float f1;
    int i1;
};

struct s3 {
    char c, e;
};

bool test_create(ecs::registry& reg) {
    std::cout << "Test creating entities..." << std::endl;
    auto a = reg.create<s1, s3>({1, 2}, {92, 93});
    auto b = reg.create<s1, s3>({7, 3}, {75, 76});
    auto c = reg.create<s2>({});
    return (reg.alive(a) && reg.alive(b) && reg.alive(c));
}

bool test_delete(ecs::registry& reg) {
    std::cout << "Test deleting entities..." << std::endl;
    auto a = reg.create<s1, s3>({1, 2}, {92, 93});
    auto b = reg.create<s1, s3>({7, 3}, {75, 76});
    auto c = reg.create<s2>({});

    reg.destroy(a);

    return !reg.alive(a);
}

bool test_get(ecs::registry& reg) {
    std::cout << "Testing getter functions..." << std::endl;
    auto a = reg.create<s2, s3>({0.345f, -45}, {'e', 'f'});
    s3& a_ref_s3 = reg.get<s3>(a);

    auto b = reg.create<s2, s3>({0.678f, -9}, {'g', 'k'});
    std::tuple<s2&, s3&> b_ref_s2_s3 = reg.get<s2&, s3&>(b);

    return (a_ref_s3.c == 'e' && std::get<0>(b_ref_s2_s3).f1 == 0.678f);
}

int main() {
    ecs::registry reg;
    std::vector<std::function<bool(ecs::registry& reg)>> test_functions = {
        test_create, test_delete, test_get
    };
    uint32_t passed;

    for(auto& test : test_functions) {
        passed += test(reg);
    }

    std::cout << "Passed " << passed << "/" << test_functions.size() << " tests!" << std::endl;
    return 0;
}