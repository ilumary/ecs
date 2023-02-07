#include <iostream>
#include <functional>

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
    std::cout << "Testing creating entities..." << std::endl;
    auto a = reg.create<s1, s3>({1, 2}, {92, 93});
    auto b = reg.create<s1, s3>({7, 3}, {75, 76});
    auto c = reg.create<s2>({});
    return (reg.alive(a) && reg.alive(b) && reg.alive(c));
}

bool test_delete(ecs::registry& reg) {
    std::cout << "Testing deleting entities..." << std::endl;
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

bool test_has(ecs::registry& reg) {
    std::cout << "Testing has function..." << std::endl;
    auto a = reg.create<s2, s3>({}, {});
    return reg.has<s2>(a);
}

bool test_func(ecs::registry& reg) {
    std::cout << "Testing functions..." << std::endl;

    reg.each([](const s1& ref_s1, const s3& ref_s3){
        //std::cout << ref_s1.i1 << " " << ref_s3.c << std::endl;
    });

    return true;
};

bool test_view(ecs::registry& reg) {
    std::cout << "Testing views..." << std::endl;

    for(const auto& [ref_s1, ref_s3] : reg.view<const s1&, const s3&>().each()) {
        //std::cout << ref_s1.i1 << " " << ref_s3.c << std::endl;
    }

    return true;
};

bool test_size(ecs::registry& reg) {
    std::cout << "Testing size..." << std::endl;

    auto view = reg.view<const s1&, const s3&>();

    return (view.size() == 3);
};

int main() {
    ecs::registry reg;
    std::vector<std::function<bool(ecs::registry& reg)>> test_functions = {
        test_create, test_delete, test_get, test_has, test_view, test_func, test_size
    };
    uint32_t passed = 0;

    for(auto& test : test_functions) {
        passed += test(reg);
    }

    std::cout << "Passed " << passed << "/" << test_functions.size() << " tests!" << std::endl;
    return 0;
}