# The Magma Parse Tool (MPT)
 A tool for parsing strings and applying scripted modifications to them.

## Table of Contents
- [Introduction](#introduction)
- [Installation](#installation)
- [Usage](#usage)


## Introduction
The Magma Parse Tool (MPT) is a tool for parsing strings and applying scripted modifications to them. It is designed to be used in a variety of applications, such as text processing, data extraction, and code generation.

The most common application (and one of the main reasons for its creation) is cross-compiling code from one language to another.


## Installation
MPT is written in C++ as a single header file. To use it, simply include the header file in your project and you're good to go.

**Note:** MPT requires C++17 or later.


## Usage
MPT is designed to be easy to use. It provides a simple API for parsing strings and writing "scripts" (or, as they are called in MPT, "rules") to modify those strings.

The basic usage of MPT is as follows:

**1** Create a `System` object.

```cpp
mgm::System mpt;
```

**2.0** Add rules to the `rules` vector in the `System` object.

**2.1** Each rule is, in turn, a list of `Word` objects.
Words can be `DIRECT`, `GENERIC`, `EXPAND`:
- `DIRECT` words are matched directly against the input string. Any `DIRECT` word must match the input string exactly. They are mainly used as delimiters between part of Rules.
- `GENERIC` words are matched against the input string, and their value is set to the sections of the input string that they match. `GENERIC` words cannot set restrictions for the type of value, or the number of values they can hold. Instead, delimiter `DIRECT` words are used to separate the values, and "organize" the `GENERIC` words.
- `EXPAND` words are a string representing how each `GENERIC` word should be expanded. Something like `hello $world` would expand the `GENERIC` word `world` to the value that it holds. For example, the generic word `world` could hold the value `, Andrew`, so the result of expanding the rule would be `hello , Andrew`. Notice the comma and space before `Andrew`. Remember that `GENERIC` words can hold as many values of any type, and expanding them just copies the raw value of the `GENERIC` word.
- There are other more advanced types, and the documentation for those can be found in the library itself.

**2.2** In addition to their `type` words also have an `optional_type` and a `repeat_type`. These are used to set restrictions for whether the word is optional or mandatory, if it can repeat or not, or if it's part of a bigger block of repeating words.

For example, a rule for a callable function could have between the parenthesis a block of 2 repeating words, the generic `value` and the delimiter word `,`.

**2.3** Applying all the types of a word is done within the constructor for each word. It can be done manually by storing information about the word in the first 3 characters of the string, or by using the automated `Word` constructor.

This code is part of the `Word` struct:

```cpp
enum class Type {
    DIRECT,
    GENERIC,
    EXPAND,
    ERROR_MESSAGE_SET,
    ERROR_FIX_SET
};
enum class OptionalType {
    MANDATORY,
    OPTIONAL,
    OPTIONAL_LIST_MANDATORY_ONE
};
enum class RepeatType {
    ONCE,
    REPEAT,
    REPEAT_SINGLE
};
Word(const std::string name, const OptionalType optional, const RepeatType repeat, const Type type);
```

**2.4** Actually adding the rule to the `System` object is done by appending the rule to the `rules` vector in the `System` object.

```cpp
// This rule replaces the word "word" with whatever the generic word evaluates to.
mpt.rules.push_back(
    Rule{
        Word("hello", OptionalType::MANDATORY, RepeatType::ONCE, Type::DIRECT),
        Word("world", OptionalType::MANDATORY, RepeatType::ONCE, Type::GENERIC),
        Word("!", OptionalType::MANDATORY, RepeatType::ONCE, Type::DIRECT)
        Word("hello $world", OptionalType::MANDATORY, RepeatType::ONCE, Type::EXPAND)
    }
);

// This rule just prints out each parameter in a function call on a new line.
mpt.rules.push_back(
    Rule{
        Word("func", OptionalType::MANDATORY, RepeatType::ONCE, Type::DIRECT),
        Word("(", OptionalType::MANDATORY, RepeatType::ONCE, Type::DIRECT),
        Word("value", OptionalType::MANDATORY, RepeatType::REPEAT, Type::GENERIC),
        Word(",", OptionalType::MANDATORY, RepeatType::REPEAT, Type::DIRECT),
        Word(")", OptionalType::MANDATORY, RepeatType::ONCE, Type::DIRECT)
        Word("$($value\n)", OptionalType::MANDATORY, RepeatType::ONCE, Type::EXPAND)
    }
);
```

**3.0** Applying the rules to the string only requires a single call to the `parse` function in the system object.

```cpp
std::string input = "hello world!";
std::string output = mpt.parse(input).result();
```

**3.1** You might notice the use of the `result` function. This is because `parse` doesn't just return a string, but rather a `Result` object, which can either be an std::string on success, or an `std::vector<CompilationError>` on failure. The `result` function is used to extract the string from the `Result` object, and throws if the `Result` object is an error.

```cpp
std::string input = "hello world!";
Result result = mpt.parse(input);
if (result.is_error()) {
    std::vector<CompilationError> errors = result.error();
    for (auto error : errors) {
        std::cout << error.message << std::endl;
    }
} else {
    std::string output = result.result();
    std::cout << output << std::endl;
}
```

The `CompilationError` struct has a few fields, including its severity, a message and an error code, an optional custom fix, a location in the input string (as an index), and the line and column of the error.

```cpp
struct CompilationError {
    enum class Severity {
        MESSAGE,
        WARNING,
        ERROR,
        SYSTEM_ERROR
    } severity{};
    Source::SourcePos pos{};
    size_t code{};
    std::string message{};
    std::string fix{};

    ...... // Other fields and methods
};
```


**4** Rules also have the option to, instead of an expand word at the end, have a custom expand function. This function should have as parameters mutable references to the system and the source string, and a const reference to a map of generic word names, and all of their values stored in a vector of strings. The function should return a result object, with either a string on success, or a CompilationError on failure.

```cpp
mgm::Result<std::string, mgm::System::CompilationError> custom_expand(mgm::System& system, mgm::System::Source& src, const std::unordered_map<std::string, std::vector<std::string>>& found_words);
```

The `Source` struct is similar to an `std::string`, but with an extra `pos` field representing the current position in the string. This field contains the actual position as an index, and the line and column of the position.

**IMPORTANT** The `Source` object isn't guaranteed to be the original source string, and the method `make_unique` has the potential to break it by making the source no longer be a simple reference to the original string. This isn't the case in the current version, but future updates might make changes without it being reflected in this documentation.

To avoid any problems caused by this, use only the const methods of the `Source` object, or make a copy of the source string before using the `make_unique` method.
