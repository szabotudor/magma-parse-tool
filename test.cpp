#include <iostream>
#include <string>
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


mgm::Result<std::string, mgm::System::CompilationError> shader_convert(mgm::System& system, mgm::System::Source& src, const std::unordered_map<std::string, std::vector<std::string>>& found_words) {
    std::string res = "#version 450 core\n";
    for (const auto& word : found_words.at("var")) {
        const auto parsed_word = system.parse(word);
        if (parsed_word.is_error()) {
            for (const auto& err : parsed_word.error())
                std::cerr << "Error at " << err.pos.line << ':' << err.pos.column << "\n\t" << err.message << std::endl;
            return mgm::System::CompilationError{parsed_word.error()[0]};
        }
        res += parsed_word.result() + '\n';
    }
    return res;
}


int main() {
    const auto test = load_file("test.mmd");

    mgm::System mp{};

    mp.rules.emplace_back(
        &shader_convert,
        "^  vertex",
        "^  fragment",
        "   {",
        "   vars:",
        " *$var",
        " * ;",
        "   code:",
        " *$code",
        " * ;",
        "   }"
    );
    mp.rules.emplace_back(
        "   var",
        "  $type",
        "  $name",
        "  +\"uniform $type $name;\""
    );
    mp.rules.emplace_back(
        "   buffer",
        "  $type",
        "  $name",
        "  +\"layout(std140, location = $EXPAND_COUNT) buffer $name { $type $name[]; };\""
    );

    const auto bytecode = mp.parse(test);
    if (bytecode.is_error())
        for (const auto& err : bytecode.error())
            std::cerr << "Error at " << err.pos.line << ':' << err.pos.column << "\n\t" << err.message << std::endl;
    else {
        std::cout << bytecode.result() << std::endl;
    }

    return 0;
}
