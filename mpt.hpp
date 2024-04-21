#pragma once
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>


namespace mgm {
    class System {
      public:
        struct Source {
            struct SourceData {
                char* data = nullptr;
                const SourceData* parent = nullptr;
                std::vector<SourceData*> refs{};
                size_t size = 0;

                SourceData() = default;

                SourceData(const SourceData& other)
                    : data{other.data}, size{other.size}, parent{other.parent ? other.parent : &other} {
                    if (parent)
                        const_cast<SourceData*>(parent)->refs.emplace_back(this);
                }
                SourceData(SourceData&& other) : data{other.data}, size{other.size}, parent{other.parent} {
                    other.data = nullptr;
                    other.parent = nullptr;
                    other.refs.clear();
                    other.size = 0;
                }
                SourceData& operator=(const SourceData& other) {
                    if (this == &other)
                        return *this;
                    this->~SourceData();
                    new (this) SourceData{other};
                    return *this;
                }
                SourceData& operator=(SourceData&& other) {
                    if (this == &other)
                        return *this;
                    this->~SourceData();
                    new (this) SourceData{std::move(other)};
                    return *this;
                }

                SourceData(const char* const& data_to_copy, const size_t size) : data{new char[size + 1]}, size{size + 1} {
                    memcpy(this->data, data_to_copy, this->size);
                }

                char& operator[](size_t i) {
                    if (parent)
                        make_unique();
                    return data[i];
                }
                const char& operator[](size_t i) const { return data[i]; }

                bool operator==(const SourceData& other) const {
                    if (size != other.size)
                        return false;
                    for (size_t i = 0; i < size; i++)
                        if (data[i] != other.data[i])
                            return false;
                    return true;
                }
                bool operator!=(const SourceData& other) const { return !(*this == other); }

                void make_unique() {
                    if (parent == nullptr)
                        return;
                    char* new_data = new char[size];
                    memcpy(new_data, data, size);
                    data = new_data;
                    const_cast<SourceData*>(parent)->refs.erase(std::find(parent->refs.begin(), parent->refs.end(), this));
                    parent = nullptr;
                }

                SourceData sub_source(const size_t pos) const { return SourceData{data + pos, size - pos}; }

                std::string substr(const size_t pos, const size_t len) const { return std::string{data + pos, len}; }

                ~SourceData() {
                    if (parent)
                        const_cast<SourceData*>(parent)->refs.erase(std::find(parent->refs.begin(), parent->refs.end(), this));
                    else if (!refs.empty())
                        for (auto& ref : refs) ref->make_unique();
                    else {
                        delete[] data;
                        new (this) SourceData{};
                    }
                }
            };

            struct SourcePos {
                size_t pos, line, column;

                SourcePos(size_t pos = 0, size_t line = 1, size_t column = 1) : pos{pos}, line{line}, column{column} {};

                bool operator==(const SourcePos& other) const {
                    return pos == other.pos && line == other.line && column == other.column;
                }
                bool operator!=(const SourcePos& other) const { return !(*this == other); }
            };

            SourceData source{};
            SourcePos pos{};

            Source() = default;
            Source(const Source& other) : source{other.source}, pos{other.pos} {}
            Source(Source&& other) : source{std::move(other.source)}, pos{other.pos} {}
            Source& operator=(const Source& other) {
                if (this == &other)
                    return *this;
                this->~Source();
                new (this) Source{other};
                return *this;
            }
            Source& operator=(Source&& other) {
                if (this == &other)
                    return *this;
                this->~Source();
                new (this) Source{std::move(other)};
                return *this;
            }

            Source(const std::string& source, const SourcePos& pos = SourcePos{})
                : source{source.c_str(), source.size() + 1}, pos{pos} {}

            Source& operator++() {
                if (reached_end())
                    return *this;

                if (source[pos.pos] == '\n') {
                    ++pos.line;
                    pos.column = 1;
                }
                else {
                    ++pos.column;
                }
                ++pos.pos;
                return *this;
            }
            Source operator++(int) {
                Source res = *this;
                ++(*this);
                return res;
            }

            Source& operator+=(size_t i) {
                while (!reached_end() && i > 0) {
                    ++(*this);
                    --i;
                }
                return *this;
            }
            Source operator+(size_t i) const {
                Source res = *this;
                res += i;
                return res;
            }

            size_t size() const { return source.size - 1; }
            bool empty() const { return source.size == 0; }
            bool reached_end() const { return pos.pos >= size() - 1; }

            char& operator*() { return (*this)[pos.pos]; }
            const char& operator*() const { return (*this)[pos.pos]; }

            char& operator[](size_t i) { return source[i]; }
            const char& operator[](size_t i) const { return source[i]; }

            bool operator==(const Source& other) const {
                if (pos != other.pos)
                    return false;
                return source == other.source;
            }
            bool operator!=(const Source& other) const { return !(*this == other); }

            bool matches(const std::string& str) const {
                if (str.size() > size() - pos.pos)
                    return false;
                for (size_t i = 0; i < str.size(); i++)
                    if (source[pos.pos + i] != str[i])
                        return false;
                return true;
            }
        };
        struct Error {
            int64_t code{};
            std::string message{};
        };
        struct CompilationError {
            enum class Severity { MESSAGE, WARNING, ERROR, SYSTEM_ERROR } severity{};
            Source::SourcePos pos{};
            size_t code{};
            std::string message{};
            std::string fix{};

            CompilationError() = default;

            CompilationError(const Source::SourcePos& pos, const std::string& message, Severity severity = Severity::ERROR,
                             const std::string& fix = "")
                : pos{pos}, message{message}, severity{severity}, fix{fix} {}

            CompilationError(const CompilationError& other) = default;
            CompilationError(CompilationError&& other) = default;
            CompilationError& operator=(const CompilationError& other) = default;
            CompilationError& operator=(CompilationError&& other) = default;

            ~CompilationError() = default;
        };
        template<typename T, typename E = Error>
        struct Result {
            union _Result {
                T value;
                E error;
                _Result() {}
                ~_Result() {}
            } _result{};
            bool _is_error = false;

            bool is_error() const { return _is_error; }

            template<std::enable_if_t<!std::is_same_v<T, E>, bool> = true> Result(T&& value) : _is_error{false} {
                new (&_result.value) T{std::move(value)};
            }
            template<std::enable_if_t<!std::is_same_v<T, E>, bool> = true> Result(const T& value) : _is_error{false} {
                new (&_result.value) T{value};
            }
            template<std::enable_if_t<!std::is_same_v<T, E>, bool> = true> Result(const E& error) : _is_error{true} {
                new (&_result.error) E{error};
            }

          private:
            void copy(const Result& res) {
                if (_is_error)
                    new (&_result.error) E{res._result.error};
                else
                    new (&_result.value) T{res._result.value};
            }
            void move(Result& res) {
                if (_is_error)
                    new (&_result.error) E{std::move(res._result.error)};
                else
                    new (&_result.value) T{std::move(res._result.value)};
            }

          public:
            Result(const Result& res) : _is_error{res._is_error} { copy(res); }
            Result(Result&& res) : _is_error{res._is_error} { move(res); }
            Result& operator=(const Result& res) {
                if (this == &res)
                    return *this;
                this->~Result();
                _is_error = res._is_error;
                copy(res);
                return *this;
            }
            Result& operator=(Result&& res) {
                if (this == &res)
                    return *this;
                this->~Result();
                _is_error = res._is_error;
                move(res);
                return *this;
            }

            static Result from_error(const E& error) {
                Result res{};
                res._is_error = true;
                new (&res._result.error) E{error};
                return res;
            }
            static Result from_value(T&& value) {
                Result res{};
                res._is_error = false;
                new (&res._result.value) T{std::move(value)};
                return res;
            }
            static Result from_value(const T& value) {
                Result res{};
                res._is_error = false;
                new (&res._result.value) T{value};
                return res;
            }

            Result() {}

            T& result() {
#ifdef ALLOW_THROW
                if (is_error())
                    throw std::runtime_error{_result.error.message.data()};
#endif
                return _result.value;
            }
            const T& result() const {
#ifdef ALLOW_THROW
                if (is_error())
                    throw std::runtime_error{"Result is error"};
#endif
                return _result.value;
            }
            const E& error() const { return _result.error; }

            ~Result() {
                if (is_error())
                    _result.error.~E();
                else
                    _result.value.~T();
            }
        };

        using GenericValueMap = std::unordered_map<std::string, std::vector<std::string>>;

      private:
        static bool is_whitespace(const char c) { return c == ' ' || c == '\n' || c == '\t'; }
        static bool is_num(const char c) { return c >= '0' && c <= '9'; }
        static bool is_alpha(const char c) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'; }
        static bool is_sym(const char c) {
            return c >= '!' && c <= '/' || c >= ':' && c <= '@' || c >= '[' && c <= '`' || c >= '{' && c <= '~';
        }

        static std::pair<size_t, size_t> get_full_brace(const Source& str) {
            std::pair<size_t, size_t> res{str.pos.pos, str.pos.pos};
            const char beg = *str;
            char end{};
            switch (beg) {
                case '(':
                    end = ')';
                    break;
                case '[':
                    end = ']';
                    break;
                case '{':
                    end = '}';
                    break;
                case '<':
                    end = '>';
                    break;
                default:
                    return {};
            }
            int st = 1;

            while (res.second < str.size() && st >= 1) {
                ++res.second;
                if (str[res.second] == beg)
                    ++st;
                if (str[res.second] == end)
                    --st;
            }
            if (str[res.second] == end)
                ++res.second;
            return res;
        }
        static std::pair<size_t, size_t> get_first_word(const Source& str, bool full_brace) {
            if (str.empty())
                return {};
            std::pair<size_t, size_t> res{str.pos.pos, str.pos.pos};
            enum _Mode { WHITESPACE, NUM, ALPHA, SYM } mode = WHITESPACE;

            while (str[res.first] != '\0') {
                if (!is_whitespace(str[res.first])) {
                    if (is_num(str[res.first]))
                        mode = NUM;
                    else if (is_alpha(str[res.first]))
                        mode = ALPHA;
                    else if (is_sym(str[res.first]))
                        mode = SYM;
                    break;
                }
                ++res.first;
                ++res.second;
            }

            switch (mode) {
                // error
                case WHITESPACE: {
                    return {};
                }
                case NUM: {
                    while ((is_num(str[res.second]) || str[res.second] == '.' || str[res.second] == 'u' ||
                            str[res.second] == 'i' || str[res.second] == 'f') &&
                           str[res.second] != '\0')
                        ++res.second;
                    return res;
                    break;
                }
                case ALPHA: {
                    while ((is_alpha(str[res.second]) || is_num(str[res.second]) || str[res.second] == '_') &&
                           str[res.second] != '\0')
                        ++res.second;
                    return res;
                    break;
                }
                case SYM: {
                    switch (str[res.first]) {
                        case '(':
                        case '[':
                        case '{':
                        case '<': {
                            if (full_brace) {
                                const auto brace = get_full_brace(str + (res.first - str.pos.pos));
                                return {res.first, brace.second};
                            }
                            return {res.first, res.second + 1};
                        }
                        case '+':
                        case '-':
                        case '*':
                        case '&':
                        case '|':
                        case '=':
                            if (str[res.first + 1] == str[res.first])
                                return {res.first, res.second + 2};
                        case '/':
                        case '^':
                        case '%': {
                            if (str[res.first + 1] == '=')
                                return {res.first, res.second + 2};
                            return {res.first, res.second + 1};
                        }
                        case '"': {
                            ++res.second;
                            while ((str[res.second] != '"' || (str[res.second] == '"' && str[res.second - 1] == '\\')) &&
                                   str[res.second] != '\0')
                                ++res.second;
                            if (str[res.second] == '"')
                                ++res.second;
                            return res;
                        }
                        default: {
                            return {res.first, res.second + 1};
                        }
                    }
                    break;
                }
            }
            return {};
        }

      public:
        struct Rule {
            struct Word {
                enum class Type { DIRECT, GENERIC, EXPAND, ERROR_MESSAGE_SET, ERROR_FIX_SET };
                enum class OptionalType { MANDATORY, OPTIONAL, OPTIONAL_LIST_MANDATORY_ONE };
                enum class RepeatType { ONCE, REPEAT, REPEAT_SINGLE };
                std::string word{};

                bool empty() const { return word.empty(); }
                Result<OptionalType> optional() const {
                    if (empty())
                        return Error{-1, "Word is empty"};
                    switch (word[0]) {
                        case ' ':
                            return OptionalType::MANDATORY;
                        case '?':
                            return OptionalType::OPTIONAL;
                        case '^':
                            return OptionalType::OPTIONAL_LIST_MANDATORY_ONE;
                        default:
                            return Error{-1, "Invalid optional type"};
                    }
                }
                Result<RepeatType> repeat() const {
                    if (empty())
                        return Error{-1, "Word is empty"};
                    switch (word[1]) {
                        case ' ':
                            return RepeatType::ONCE;
                        case '*':
                            return RepeatType::REPEAT;
                        case '#':
                            return RepeatType::REPEAT_SINGLE;
                        default:
                            return Error{-1, "Invalid repeat type"};
                    }
                }
                Result<Type> type() const {
                    if (empty())
                        return Error{-1, "Word is empty"};
                    switch (word[2]) {
                        case ' ':
                            return Type::DIRECT;
                        case '$':
                            return Type::GENERIC;
                        case '+':
                            return Type::EXPAND;
                        case '!':
                            return Type::ERROR_MESSAGE_SET;
                        case '?':
                            return Type::ERROR_FIX_SET;
                        default:
                            return Error{-1, "Invalid type"};
                    }
                }

                Word(const Word& other) : word{other.word} {}
                Word(Word&& other) : word{std::move(other.word)} {}
                Word& operator=(const Word& other) {
                    if (this == &other)
                        return *this;
                    word = other.word;
                    return *this;
                }
                Word& operator=(Word&& other) {
                    if (this == &other)
                        return *this;
                    word = std::move(other.word);
                    return *this;
                }

                Word(const std::string& str) : word{str} {
                    if (optional().is_error() || repeat().is_error() || type().is_error())
                        word.clear();
                    if (type().result() == Type::EXPAND &&
                        (repeat().result() != RepeatType::ONCE || optional().result() != OptionalType::MANDATORY))
                        word.clear();
                }

                Word(const std::string name, const OptionalType optional, const RepeatType repeat, const Type type)
                    : word{"   " + name} {
                    switch (optional) {
                        case OptionalType::MANDATORY:
                            break;
                        case OptionalType::OPTIONAL:
                            word[0] = '?';
                            break;
                        case OptionalType::OPTIONAL_LIST_MANDATORY_ONE:
                            word[0] = '^';
                            break;
                    }
                    switch (repeat) {
                        case RepeatType::ONCE:
                            break;
                        case RepeatType::REPEAT:
                            word[1] = '*';
                            break;
                        case RepeatType::REPEAT_SINGLE:
                            word[1] = '#';
                            break;
                    }
                    switch (type) {
                        case Type::DIRECT:
                            break;
                        case Type::GENERIC:
                            word[2] = '$';
                            break;
                        case Type::EXPAND:
                            word[2] = '+';
                            break;
                        case Type::ERROR_MESSAGE_SET:
                            word[2] = '!';
                            break;
                        case Type::ERROR_FIX_SET:
                            word[2] = '?';
                            break;
                    }
                }
            };

            std::vector<Word> words{};

            size_t num_words() const { return words.size() - 1; }

            Rule() = default;

            template<typename... Ts, std::enable_if_t<(std::is_constructible_v<Word, Ts> && ...), bool> = true>
            Rule(Ts&&... args) : words{Word{std::forward<Ts>(args)}...} {
                const auto valid = is_valid();
                if (is_valid().is_error()) {
                    words.clear();
                    std::cerr << "Invalid rule: " << valid.error().message << std::endl;
                }
            }

            Result<bool> is_valid() const {
                if (words.empty())
                    return Error{1, "Rule is empty"};

                const auto last_word_type = words.back().type();
                if (last_word_type.is_error())
                    return Error{2, "Invalid rule. Contains malformed word"};
                if (last_word_type.result() != Word::Type::EXPAND)
                    return Error{3, "Invalid rule. Last word must be of type EXPAND"};

                const auto last_word_repeat = words[words.size() - 1].repeat();
                if (last_word_repeat.is_error())
                    return Error{2, "Invalid rule. Contains malformed word"};
                if (last_word_repeat.result() != Word::RepeatType::ONCE)
                    return Error{5, "Invalid rule. Last word cannot be repeating"};

                for (size_t i = 0; i < words.size(); i++) {
                    const auto& word = words[i];
                    if (word.type().is_error())
                        return Error{2, "Invalid rule. Contains malformed word"};

                    if (word.repeat().result() != Word::RepeatType::ONCE &&
                        words[i + 1].optional().result() != Word::OptionalType::MANDATORY)
                        return Error{6, "Invalid rule. Any repeating word (or list of repeating words) cannot be followed by "
                                        "an optional word"};

                    if (word.type().result() == Word::Type::GENERIC)
                        for (size_t j = i + 1; j < words.size(); j++)
                            if (words[j].type().result() == Word::Type::GENERIC)
                                if (words[j].word.substr(3) == word.word.substr(3))
                                    return Error{7, "Invalid rule. Contains duplicate generic word name"};
                }
                return true;
            }

            struct WordMatch {
                size_t id{};
                std::pair<size_t, size_t> match{};
            };

          private:
            Result<std::pair<size_t, size_t>> ensure_word_match(const Source& str, const size_t word_id,
                                                                size_t* found_word_b_return = nullptr) const {
                const auto& word = words[word_id];
                const auto type = word.type().result();
                switch (type) {
                    case Word::Type::DIRECT: {
                        if (found_word_b_return) {
                            const auto first_word = get_first_word(str, true);
                            *found_word_b_return = first_word.second;
                        }
                        const auto word_desc = get_first_word(str, false);
                        if (word_desc.second - word_desc.first == 0)
                            return Error{-1, "Expected word"};
                        if (!(str + (word_desc.first - str.pos.pos)).matches(word.word.substr(3)))
                            return Error{-1, "Word does not match expected word"};
                        return std::pair{word_desc.first, word_desc.first + word.word.size() - 3};
                    }
                    case Word::Type::GENERIC: {
                        if (word_id == num_words() - 1) {
                            const auto first_word = get_first_word(str, true);
                            if (first_word.second - first_word.first == 0)
                                return Error{-1, "Expected word"};
                            if (found_word_b_return)
                                *found_word_b_return = first_word.second;
                            return first_word;
                        }
                        auto first_word = get_first_word(str, true);
                        auto str_cpy = str;
                        if (first_word.second - first_word.first == 0)
                            return Error{-1, "Expected word"};
                        size_t i = first_word.second;
                        size_t next_word_id = word_id + 1;
                        size_t backup_word = next_word_id;
                        while (words[backup_word].repeat().result() == Word::RepeatType::REPEAT) ++backup_word;
                        System::Result<std::pair<size_t, size_t>> next_word_match = Error{};
                        do {
                            const size_t _i = i;
                            str_cpy += i - str_cpy.pos.pos;
                            next_word_match = ensure_word_match(str_cpy, next_word_id, &i);
                            if (word.repeat().result() == Word::RepeatType::REPEAT && next_word_match.is_error())
                                next_word_match = ensure_word_match(str_cpy, backup_word, &i);
                            if (str_cpy.reached_end() || _i == i)
                                return Error{-1, "Reached end of string without finding next word"};
                        }
                        while (next_word_match.is_error());
                        i = next_word_match.result().first;
                        if (i > 0)
                            while (is_whitespace(str[i - 1])) --i;
                        first_word.second = i;
                        if (first_word.second < first_word.first)
                            return Error{-1, "Expected word"};
                        if (found_word_b_return)
                            *found_word_b_return = i;
                        return first_word;
                    }
                    case Word::Type::EXPAND: {
                        if (found_word_b_return)
                            *found_word_b_return = (size_t)str.size();
                        size_t i = 0;
                        while (is_whitespace(str[i])) ++i;
                        return std::pair{i, i};
                    }
                    default: {
                        return Error{-1, "Word is not matchable"};
                    }
                };
                return {};
            }

          public:
            Result<std::vector<WordMatch>, std::pair<std::vector<WordMatch>, CompilationError>> match(const Source& str) const {
                if (str.empty())
                    return std::pair{
                        std::vector<WordMatch>{},
                        CompilationError{{}, "String is empty", CompilationError::Severity::ERROR}
                    };
                const auto valid = is_valid();
                if (valid.is_error())
                    return std::pair{
                        std::vector<WordMatch>{},
                        CompilationError{{}, valid.error().message, CompilationError::Severity::SYSTEM_ERROR}
                    };

                std::vector<WordMatch> res{};
                res.reserve(words.size());

                size_t i = 0;
                size_t pos = 0;
                bool repeating = 0;

                while (i < words.size()) {
                    auto word_match = ensure_word_match(str + pos, i);
                    if (word_match.is_error()) {
                        if (words[i].empty()) {
                            ++i;
                            continue;
                        }
                        if (words[i].repeat().result() == Word::RepeatType::REPEAT_SINGLE) {
                            if (!repeating && words[i].optional().result() != Word::OptionalType::OPTIONAL)
                                return std::pair{
                                    res, CompilationError{(str + pos).pos, "Single repeating word not found",
                                                          CompilationError::Severity::ERROR}
                                };
                            repeating = false;
                            ++i;
                            continue;
                        }
                        if (repeating) {
                            if (words[i].repeat().result() == Word::RepeatType::REPEAT) {
                                while (words[i].repeat().result() == Word::RepeatType::REPEAT) ++i;
                                word_match = ensure_word_match(str + pos, i);
                                if (!word_match.is_error())
                                    continue;
                            }
                            if (i > 0) {
                                while (words[i - 1].repeat().result() == Word::RepeatType::REPEAT) {
                                    --i;
                                    if (i == 0)
                                        break;
                                }
                                word_match = ensure_word_match(str + pos, i);
                                if (!word_match.is_error())
                                    continue;
                            }
                            return std::pair{
                                res, CompilationError{(str + pos).pos,
                                                      "Repeating word not found or no closer was found after repeating words", CompilationError::Severity::ERROR}
                            };
                        }
                        if (words[i].optional().result() == Word::OptionalType::OPTIONAL) {
                            ++i;
                            continue;
                        }
                        if (words[i].optional().result() == Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE) {
                            if (words[i + 1].optional().result() != Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE)
                                return std::pair{
                                    res,
                                    CompilationError{(str + pos).pos, "Word should match at least one option in optional list",
                                                     CompilationError::Severity::ERROR}
                                };
                            while (words[i].optional().result() == Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE) ++i;
                            continue;
                        }
                        return std::pair{
                            res,
                            CompilationError{(str + pos).pos, std::string{"Word \""} + words[i].word.substr(3) + "\" not found",
                                             CompilationError::Severity::ERROR}
                        };
                    }
                    pos = word_match.result().second;
                    res.emplace_back(WordMatch{i, word_match.result()});

                    if (words[i].optional().result() == Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE)
                        while (words[i + 1].optional().result() == Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE) ++i;

                    switch (words[i].repeat().result()) {
                        case Word::RepeatType::ONCE:
                            ++i;
                            repeating = false;
                            break;
                        case Word::RepeatType::REPEAT:
                            ++i;
                            repeating = true;
                            break;
                        case Word::RepeatType::REPEAT_SINGLE:
                            repeating = true;
                            break;
                    }
                }
                return res;
            }
        };

        struct Extension {
            Extension() = default;

            virtual Result<std::string> operator()(System& system, const GenericValueMap& found_words,
                                                   const std::string& params = "") = 0;

            virtual ~Extension() = default;
        };

      private:
        struct ExtensionContainer {
          private:
            Extension* extension{};
            std::function<Extension*(Extension* original)> clone_func{};

          public:
            ExtensionContainer() = default;
            ExtensionContainer(const ExtensionContainer& other)
                : extension{other.clone_func(other.extension)}, clone_func{other.clone_func} {}
            ExtensionContainer(ExtensionContainer&& other) : extension{other.extension}, clone_func{other.clone_func} {
                other.extension = nullptr;
                other.clone_func = nullptr;
            }
            ExtensionContainer& operator=(const ExtensionContainer& other) {
                if (this == &other)
                    return *this;
                delete extension;
                extension = other.clone_func(other.extension);
                clone_func = other.clone_func;
                return *this;
            }
            ExtensionContainer& operator=(ExtensionContainer&& other) {
                if (this == &other)
                    return *this;
                delete extension;
                extension = other.extension;
                clone_func = other.clone_func;
                other.extension = nullptr;
                other.clone_func = nullptr;
                return *this;
            }

            template<typename T, typename... Ts,
                     std::enable_if_t<std::is_base_of_v<Extension, T> && std::is_constructible_v<T, Ts...>, bool> = true>
            ExtensionContainer& emplace(Ts&&... args) {
                delete extension;
                extension = new T{std::forward<Ts>(args)...};
                clone_func = [](Extension* original) {
                    return new T{*dynamic_cast<T*>(original)};
                };
                return *this;
            }

            Result<std::string> operator()(System& system, const GenericValueMap& found_words, const std::string& params = "") {
                if (!extension)
                    return Error{-1, "Extension is empty"};
                return (*extension)(system, found_words, params);
            }

            template<typename T> T& get() { return *dynamic_cast<T*>(extension); }
            template<typename T> const T& get() const { return *dynamic_cast<T*>(extension); }

            ~ExtensionContainer() { delete extension; }
        };

      public:
        std::vector<Rule> rules{};
        std::unordered_map<std::string, ExtensionContainer> extensions{};

        template<typename T, typename... Ts,
                 std::enable_if_t<std::is_base_of_v<Extension, T> && std::is_constructible_v<T, Ts...>, bool> = true>
        void add_extension(const std::string& name, Ts&&... args) {
            extensions[name].emplace<T>(std::forward<T>(args)...);
        }

      private:
        struct ExpandCountExtension : public Extension {
            using Extension::Extension;
            size_t count{};
            std::unordered_map<std::string, size_t> counts{};
            System::Result<std::string> operator()(System& system, const System::GenericValueMap& found_words,
                                                   const std::string& params) override {
                if (params.empty())
                    return std::to_string(count++);

                const auto word = get_first_word(params, false);
                if (word.second == 0)
                    return Error{-1, "No word to expand"};
                const auto var = params.substr(word.first, word.second - word.first);

                if (var == "RESET") {
                    count = 0;
                    counts.clear();
                    return std::to_string(count++);
                }

                const auto it = counts.find(var);
                if (it == counts.end())
                    return std::to_string(counts.emplace(var, 0).first->second++);
                return std::to_string(it->second++);
            }
        };

      public:
        void enable_default_extensions() {
            extensions.clear();
            add_extension<ExpandCountExtension>("EXPAND_COUNT", ExpandCountExtension{});
        }
        System(const std::vector<Rule>& rules = {}, const std::unordered_map<std::string, ExtensionContainer>& extensions = {})
            : rules{rules}, extensions{extensions} {}
        System(const System& other) = default;
        System(System&& other) = default;
        System& operator=(const System& other) = default;
        System& operator=(System&& other) = default;

      private:
        Result<std::string> expand_generic(const std::string& str, const GenericValueMap& expand_vars) {
            const auto expr_to_expand = get_first_word(str, true);
            if (expr_to_expand.second - expr_to_expand.first == 0)
                return Error{-1, "Expected expression after $"};

            if (str[expr_to_expand.first] == '(') {
                std::vector<std::pair<std::string, Rule::Word::Type>> words_in_expr{};
                std::vector<std::pair<size_t, size_t>> exprs_to_expand{};
                size_t max_iterations = (size_t)-1;
                for (size_t i = expr_to_expand.first + 1; i < expr_to_expand.second - 1; i++) {
                    if (str[i] == '$') {
                        ++i;
                        auto word_to_expand = get_first_word(str.substr(i), true);
                        word_to_expand = std::pair{word_to_expand.first + i, word_to_expand.second + i};
                        if (str[word_to_expand.first] == '(') {
                            const auto expand_result = expand_generic(
                                str.substr(word_to_expand.first, word_to_expand.second - word_to_expand.first), expand_vars);
                            if (expand_result.is_error())
                                return expand_result.error();
                            words_in_expr.emplace_back(expand_result.result(), Rule::Word::Type::DIRECT);
                        }
                        else if (is_alpha(str[word_to_expand.first])) {
                            words_in_expr.emplace_back(
                                str.substr(word_to_expand.first, word_to_expand.second - word_to_expand.first),
                                Rule::Word::Type::GENERIC);
                            const auto& var = expand_vars.at(words_in_expr.back().first);
                            max_iterations = std::min(max_iterations, var.size());
                        }
                        else
                            return Error{-1, "Invalid expression after $"};
                        exprs_to_expand.emplace_back(word_to_expand.first - 1, word_to_expand.second);
                        i = word_to_expand.second - 1;
                    }
                }

                if (words_in_expr.size() != exprs_to_expand.size())
                    return Error{-1, "Unknown internal error"};

                std::string res{};
                size_t last_word_end = 1;
                size_t current_word = 0;
                for (size_t iteration = 0; iteration < max_iterations; iteration++) {
                    for (size_t i = 0; i < words_in_expr.size(); i++) {
                        if (exprs_to_expand[i].first > last_word_end)
                            res += str.substr(last_word_end, exprs_to_expand[i].first - last_word_end);
                        last_word_end = exprs_to_expand[i].second;

                        const auto& word = words_in_expr[i];
                        switch (word.second) {
                            case Rule::Word::Type::DIRECT: {
                                res += word.first;
                                break;
                            }
                            case Rule::Word::Type::GENERIC: {
                                res += expand_vars.at(word.first)[iteration];
                                break;
                            }
                            default:
                                return Error{-1, "Unknown internal error"};
                        }
                    }
                    if (exprs_to_expand.back().second < expr_to_expand.second - 1)
                        res += str.substr(exprs_to_expand.back().second,
                                          expr_to_expand.second - exprs_to_expand.back().second - 1);
                }
                return res;
            }

            if (is_alpha(str[expr_to_expand.first])) {
                const auto var_name = str.substr(expr_to_expand.first, expr_to_expand.second - expr_to_expand.first);
                const auto ext = extensions.find(var_name);
                if (ext != extensions.end()) {
                    auto params_expr = get_first_word(str.substr(expr_to_expand.second), true);
                    params_expr =
                        std::pair{params_expr.first + expr_to_expand.second, params_expr.second + expr_to_expand.second};
                    if (str[params_expr.first] == '(') {
                        const auto ext_result =
                            ext->second(*this, expand_vars, str.substr(params_expr.first + 1, params_expr.second - 1));
                        if (ext_result.is_error())
                            return ext_result.error();
                        return ext_result.result();
                    }
                    const auto ext_result = ext->second(*this, expand_vars, "");
                    if (ext_result.is_error())
                        return ext_result.error();
                    return ext_result.result();
                }
                const auto& expand_to = expand_vars.find(var_name);
                if (expand_to == expand_vars.end())
                    return Error{-1, '"' + var_name + '"' + " is not a variable or extension"};
                if (expand_to->second.empty())
                    return Error{-1, "Variable \"" + var_name + '"' + " has no value(s)"};
                return expand_to->second.front();
            }

            return Error{-1, "Invalid expression after $"};
        }

      public:
        Result<std::string, std::vector<CompilationError>> parse(Source str, const bool instant_fail = false) {
            std::string res{};
            std::vector<CompilationError> errors{};

            for (; !str.reached_end(); ++str) {
                if (!errors.empty() && instant_fail)
                    return errors;

                while (is_whitespace(*str)) ++str;

                if (*str == '"') {
                    const auto word = get_first_word(str, true);
                    res += str.source.substr(word.first + 1, word.second - word.first - 2);
                    if (word.second > str.pos.pos)
                        str += word.second - str.pos.pos;
                    continue;
                }

                const Rule* found_rule = nullptr;
                std::vector<Rule::WordMatch> found_words{};
                float best_match_score = 0.0f;
                CompilationError rule_match_error{0, ""};

                for (const auto& rule : rules) {
                    const auto _found_words = rule.match(str);
                    float match_score = 0.0f;
                    std::vector<Rule::WordMatch> _found_words_result{};
                    if (_found_words.is_error())
                        _found_words_result = _found_words.error().first;
                    else
                        _found_words_result = _found_words.result();

                    if (_found_words_result.empty()) {
                        if (best_match_score == 0.0f && rule_match_error.message.empty())
                            rule_match_error = _found_words.error().second;
                        continue;
                    }

                    match_score = float(_found_words_result.back().id + 1) / float(rule.words.size());

                    if (match_score == 1.0f && rule.words[rule.words.size() - 2].type().result() == Rule::Word::Type::DIRECT)
                        match_score = 2.0f;

                    if (match_score > best_match_score) {
                        found_rule = &rule;
                        found_words = _found_words_result;
                        best_match_score = match_score;
                    }
                    if (best_match_score < 1.0f)
                        rule_match_error = _found_words.error().second;
                    if (best_match_score == 2.0f)
                        break;
                }

                if (best_match_score >= 1.0f) {
                    GenericValueMap expand_vars{};
                    for (const auto& word : found_words)
                        if (found_rule->words[word.id].type().result() == Rule::Word::Type::GENERIC)
                            expand_vars[found_rule->words[word.id].word.substr(3)].emplace_back(
                                str.source.substr(word.match.first, word.match.second - word.match.first));

                    auto expand = found_rule->words.back().word.substr(3);
                    for (size_t j = 0; j < expand.size(); j++) {
                        if (expand[j] == '$') {
                            ++j;
                            auto expand_expr = get_first_word(expand.substr(j), true);
                            expand_expr = std::pair{expand_expr.first + j, expand_expr.second + j};
                            auto params_expr = get_first_word(expand.substr(expand_expr.second), false);
                            params_expr =
                                std::pair{params_expr.first + expand_expr.second, params_expr.second + expand_expr.second};
                            if (expand[params_expr.first] == '(')
                                expand_expr.second =
                                    get_first_word(expand.substr(j + expand_expr.second), true).second + j + expand_expr.second;
                            const auto expand_result = expand_generic(
                                expand.substr(expand_expr.first, expand_expr.second - expand_expr.first), expand_vars);
                            if (expand_result.is_error()) {
                                errors.emplace_back(str.pos, expand_result.error().message);
                                expand.clear();
                                break;
                            }
                            expand = expand.substr(0, j - 1) + expand_result.result() + expand.substr(expand_expr.second);
                            j = j - 1 + expand_result.result().size();
                        }
                    }
                    if (expand.empty()) {
                        str += found_words.back().match.second - str.pos.pos;
                        continue;
                    }
                    const auto parse_result = parse(expand);
                    if (parse_result.is_error()) {
                        errors.emplace_back(str.pos,
                                            "Found " + std::to_string(parse_result.error().size()) +
                                                " errors while parsing expanded string:",
                                            CompilationError::Severity::ERROR);
                        for (const auto& err : parse_result.error())
                            errors.emplace_back((str + err.pos.pos).pos, err.message, err.severity, err.fix);
                    }
                    else
                        res += parse_result.result();
                    const auto last_word_is_expand =
                        found_rule->words.back().type().result() == Rule::Word::Type::EXPAND ? 2 : 1;
                    if (found_words[found_words.size() - last_word_is_expand].match.second > str.pos.pos)
                        str += found_words[found_words.size() - last_word_is_expand].match.second - str.pos.pos;
                    continue;
                }

                if (found_words.empty()) {
                    errors.emplace_back(rule_match_error);
                    const auto word = get_first_word(str, true);
                    str += word.second - str.pos.pos;
                    continue;
                }
                if (found_words.back().match.second > str.pos.pos) {
                    errors.emplace_back(rule_match_error);
                    str += found_words.back().match.second - str.pos.pos;
                }
            }

            if (!errors.empty())
                return errors;
            return res;
        }

        ~System() = default;
    };
} // namespace mgm
