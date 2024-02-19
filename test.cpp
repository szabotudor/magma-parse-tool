#include <iostream>
#if defined(DEBUG) || defined(_DEBUG)
#define ALLOW_THROW
#endif

#include "mpt.hpp"
#include <fstream>


std::string load_file(const std::string& file) {
    std::ifstream fin{file, std::ios::ate};
    size_t len = fin.tellg();
    fin.seekg(0);
    char* str = new char[len + 1];
    fin.read(str, len);
    str[len] = '\0';
    return str;
}


int main() {
    const auto test = load_file("test.mmd");

    mgm::System mp{};

    mp.rules.emplace_back(
        "   test",
        "   (",
        " *$v",
        " * ,",
        "   )",
        "  +\"$($v/$($v), )\""
    );

    const auto bytecode = mp.parse(test);
    if (bytecode.is_error())
        for (const auto& err : bytecode.error())
            std::cerr << "Error at " << err.pos.line << ':' << err.pos.column << '\n' << err.message << std::endl;
    else {
        std::cout << bytecode.result() << std::endl;
    }

    return 0;
}
