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

struct ShaderExtension : public mgm::System::Extension {
    virtual mgm::System::Result<std::string> operator()(mgm::System& system, const mgm::System::GenericValueMap& found_words,
                                                        const std::string&) override {
        std::string res = "#version 450 core\n";
        for (const auto& word : found_words.at("var")) {
            const auto parsed_word = system.parse(word);
            if (parsed_word.is_error()) {
                for (const auto& err : parsed_word.error())
                    std::cerr << "Error at " << err.pos.line << ':' << err.pos.column << "\n\t" << err.message << std::endl;
                return mgm::System::Error{static_cast<int64_t>(parsed_word.error()[0].code), parsed_word.error()[0].message};
            }
            res += parsed_word.result() + '\n';
        }
        return res;
    }
};

int main() {
    const auto test = load_file("test.mmd");

    mgm::System mp{};
    mp.enable_default_extensions();

    mp.add_extension<ShaderExtension>("SHADER");
    mp.rules.emplace_back("^  vertex", "^  fragment", "   {", "   vars:", " *$var", " * ;", "   code:", " *$code", " * ;",
                          "   }", "  +\"$SHADER\nvoid main() {\n$($code;\n)}\"");
    mp.rules.emplace_back("   var", "  $type", "  $name", "  +\"uniform $type $name;\"");
    mp.rules.emplace_back("   buffer", "  $type", "  $name",
                          "  +\"layout(std140, location = $EXPAND_COUNT) buffer $name { $type $name[]; };\"");

    const auto bytecode = mp.parse(test);
    if (bytecode.is_error())
        for (const auto& err : bytecode.error())
            std::cerr << "Error at " << err.pos.line << ':' << err.pos.column << "\n\t" << err.message << std::endl;
    else {
        std::cout << bytecode.result() << std::endl;
    }

    return 0;
}
