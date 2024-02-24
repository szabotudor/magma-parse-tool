#pragma once
#include <any>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace mgm {
    struct Error {
        int64_t code{};
        std::string message{};
    };
    template<typename T, typename E = Error> struct Result {
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
            if (this == &res) return *this;
            this->~Result();
            _is_error = res._is_error;
            copy(res);
            return *this;
        }
        Result& operator=(Result&& res) {
            if (this == &res) return *this;
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
            if (is_error()) throw std::runtime_error{_result.error.message.data()};
#endif
            return _result.value;
        }
        const T& result() const {
#ifdef ALLOW_THROW
            if (is_error()) throw std::runtime_error{"Result is error"};
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
    template<typename T, typename U> struct Pair {
        T a;
        U b;

        Pair() : a{}, b{} {};

        Pair(const T& a, const U& b) : a{a}, b{b} {}
        Pair(T&& a, U&& b) : a{std::move(a)}, b{std::move(b)} {}

        Pair(const Pair<T, U>& p) : a{p.a}, b{p.b} {}
        Pair(Pair<T, U>&& p) : a{std::move(p.a)}, b{std::move(p.b)} {}
        Pair<T, U>& operator=(const Pair<T, U>& p) {
            if (this == &p) return *this;
            a = p.a;
            b = p.b;
            return *this;
        }
        Pair<T, U>& operator=(Pair<T, U>&& p) {
            if (this == &p) return *this;
            a = std::move(p.a);
            b = std::move(p.b);
            return *this;
        }

        template<typename... Ts, std::enable_if_t<std::is_constructible_v<T, Ts...>, bool> = true> void emplace_a(Ts&&... ts) {
            a.~T();
            new (&a) T{std::forward(ts)...};
        }
        template<typename... Us, std::enable_if_t<std::is_constructible_v<U, Us...>, bool> = true> void emplace_b(Us&&... ts) {
            b.~U();
            new (&b) U{std::forward(ts)...};
        }

        bool operator==(const Pair<T, U>& p) const { return a == p.a && b == p.b; }
        bool operator!=(const Pair<T, U>& p) const { return !(*this == p); }

        ~Pair() = default;
    };

    class System {
      public:
        struct Source {
            struct SourceData {
                char* data = nullptr;
                size_t size : (sizeof(size_t) * 8 - 1);
                bool owned : 1;

                SourceData() : data{nullptr}, size{0}, owned{false} {}

                SourceData(const SourceData& other) : data{other.data}, size{other.size}, owned{false} {}
                SourceData(SourceData&& other) : data{other.data}, size{other.size}, owned{other.owned} {
                    other.data = nullptr;
                    other.size = 0;
                    other.owned = false;
                }
                SourceData& operator=(const SourceData& other) {
                    if (this == &other) return *this;
                    this->~SourceData();
                    new (this) SourceData{other};
                    return *this;
                }
                SourceData& operator=(SourceData&& other) {
                    if (this == &other) return *this;
                    this->~SourceData();
                    new (this) SourceData{std::move(other)};
                    return *this;
                }

                SourceData(char* const& data, const size_t size) : data{data}, size{size}, owned{false} {}
                SourceData(const char* const& data_to_copy, const size_t size)
                    : data{new char[size]}, size{size + 1}, owned{true} {
                    memcpy(this->data, data_to_copy, this->size);
                }

                char& operator[](size_t i) { return data[i]; }
                const char& operator[](size_t i) const { return data[i]; }

                bool operator==(const SourceData& other) const {
                    if (size != other.size) return false;
                    for (size_t i = 0; i < size; i++)
                        if (data[i] != other.data[i]) return false;
                    return true;
                }
                bool operator!=(const SourceData& other) const { return !(*this == other); }

                void make_unique() {
                    if (owned) return;
                    char* new_data = new char[size];
                    memcpy(new_data, data, size);
                    data = new_data;
                    owned = true;
                }

                SourceData sub_source(const size_t pos) const { return SourceData{data + pos, size - pos}; }

                std::string substr(const size_t pos, const size_t len) const { return std::string{data + pos, len}; }

                ~SourceData() {
                    if (owned) delete[] data;
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
                if (this == &other) return *this;
                this->~Source();
                new (this) Source{other};
                return *this;
            }
            Source& operator=(Source&& other) {
                if (this == &other) return *this;
                this->~Source();
                new (this) Source{std::move(other)};
                return *this;
            }

            Source(const std::string& source, const SourcePos& pos = SourcePos{})
                : source{source.c_str(), source.size()}, pos{pos} {}

            Source& operator++() {
                if (reached_end()) return *this;

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

            size_t size() const { return source.size; }
            bool empty() const { return source.size == 0; }
            bool reached_end() const { return pos.pos >= size() - 1; }

            char& operator*() { return (*this)[pos.pos]; }
            const char& operator*() const { return (*this)[pos.pos]; }

            char& operator[](size_t i) {
                if (!source.owned) source.make_unique();
                return source[i];
            }
            const char& operator[](size_t i) const { return source[i]; }

            bool operator==(const Source& other) const {
                if (pos != other.pos) return false;
                return source == other.source;
            }
            bool operator!=(const Source& other) const { return !(*this == other); }

            bool matches(const std::string& str) const {
                if (str.size() > size() - pos.pos) return false;
                for (size_t i = 0; i < str.size(); i++)
                    if (source[pos.pos + i] != str[i]) return false;
                return true;
            }
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

      private:
        static bool is_whitespace(const char c) { return c == ' ' || c == '\n' || c == '\t'; }
        static bool is_num(const char c) { return c >= '0' && c <= '9'; }
        static bool is_alpha(const char c) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'; }
        static bool is_sym(const char c) {
            return c >= '!' && c <= '/' || c >= ':' && c <= '@' || c >= '[' && c <= '`' || c >= '{' && c <= '~';
        }

        static Pair<size_t, size_t> get_full_brace(const Source& str) {
            Pair<size_t, size_t> res{str.pos.pos, str.pos.pos};
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

            while (res.b < str.size() && st >= 1) {
                ++res.b;
                if (str[res.b] == beg) ++st;
                if (str[res.b] == end) --st;
            }
            if (str[res.b] == end) ++res.b;
            return res;
        }
        static Pair<size_t, size_t> get_first_word(const Source& str, bool full_brace) {
            if (str.empty()) return {};
            Pair<size_t, size_t> res{str.pos.pos, str.pos.pos};
            enum _Mode { WHITESPACE, NUM, ALPHA, SYM } mode = WHITESPACE;

            while (str[res.a] != '\0') {
                if (!is_whitespace(str[res.a])) {
                    if (is_num(str[res.a]))
                        mode = NUM;
                    else if (is_alpha(str[res.a]))
                        mode = ALPHA;
                    else if (is_sym(str[res.a]))
                        mode = SYM;
                    break;
                }
                ++res.a;
                ++res.b;
            }

            switch (mode) {
                // error
                case WHITESPACE: {
                    return {};
                }
                case NUM: {
                    while (is_num(str[res.b]) || str[res.b] == '.' || str[res.b] == 'u' || str[res.b] == 'i' ||
                           str[res.b] == 'f')
                        ++res.b;
                    return res;
                    break;
                }
                case ALPHA: {
                    while (is_alpha(str[res.b]) || is_num(str[res.b]) || str[res.b] == '_') ++res.b;
                    return res;
                    break;
                }
                case SYM: {
                    switch (str[res.a]) {
                        case '(':
                        case '[':
                        case '{':
                        case '<': {
                            if (full_brace) {
                                const auto brace = get_full_brace(str + (res.a - str.pos.pos));
                                return {res.a, brace.b};
                            }
                            return {res.a, res.b + 1};
                        }
                        case '+':
                        case '-':
                        case '*':
                        case '/':
                        case '&':
                        case '|':
                        case '^':
                        case '%': {
                            if (str[res.a + 1] == '=') return {res.a, res.b + 2};
                            return {res.a, res.b + 1};
                        }
                        case '"': {
                            ++res.b;
                            while ((str[res.b] != '"' || (str[res.b] == '"' && str[res.b - 1] == '\\')) && str[res.b] != '\0')
                                ++res.b;
                            if (str[res.b] == '"') ++res.b;
                            return res;
                        }
                        default: {
                            return {res.a, res.b + 1};
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
                    if (empty()) return Error{-1, "Word is empty"};
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
                    if (empty()) return Error{-1, "Word is empty"};
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
                    if (empty()) return Error{-1, "Word is empty"};
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
                    if (this == &other) return *this;
                    word = other.word;
                    return *this;
                }
                Word& operator=(Word&& other) {
                    if (this == &other) return *this;
                    word = std::move(other.word);
                    return *this;
                }

                Word(const std::string& str) : word{str} {
                    if (optional().is_error() || repeat().is_error() || type().is_error()) word.clear();
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
            using CustomExpandFunc = std::function<Result<std::string, CompilationError>(
                System& system, Source& source, const std::unordered_map<std::string, std::vector<std::string>>& found_words)>;

            std::vector<Word> words{};

            size_t num_words() const {
                return words.back().type().result() == Word::Type::EXPAND ? words.size() - 1 : words.size();
            }

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
                if (words.empty()) return Error{1, "Rule is empty"};

                const auto last_word_type = words.back().type();
                if (last_word_type.is_error()) return Error{2, "Invalid rule. Contains malformed word"};
                if (last_word_type.result() != Word::Type::EXPAND)
                    return Error{3, "Invalid rule. Last word must be of type EXPAND"};

                const auto last_word_repeat = words[words.size() - 1].repeat();
                if (last_word_repeat.is_error()) return Error{2, "Invalid rule. Contains malformed word"};
                if (last_word_repeat.result() != Word::RepeatType::ONCE)
                    return Error{5, "Invalid rule. Last word cannot be repeating"};

                for (size_t i = 0; i < words.size(); i++) {
                    const auto& word = words[i];
                    if (word.type().is_error()) return Error{2, "Invalid rule. Contains malformed word"};

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
                Pair<size_t, size_t> match{};
            };

          private:
            Result<Pair<size_t, size_t>> ensure_word_match(const Source& str, const size_t word_id,
                                                           size_t* found_word_b_return = nullptr) const {
                const auto& word = words[word_id];
                const auto type = word.type().result();
                switch (type) {
                    case Word::Type::DIRECT: {
                        if (found_word_b_return) {
                            const auto first_word = get_first_word(str, true);
                            *found_word_b_return = first_word.b;
                        }
                        const auto word_desc = get_first_word(str, false);
                        if (word_desc.b - word_desc.a == 0) return Error{-1, "Expected word"};
                        if (!(str + (word_desc.a - str.pos.pos)).matches(word.word.substr(3)))
                            return Error{-1, "Word does not match expected word"};
                        return Pair{word_desc.a, word_desc.a + word.word.size() - 3};
                    }
                    case Word::Type::GENERIC: {
                        if (word_id == num_words() - 1) {
                            const auto first_word = get_first_word(str, true);
                            if (first_word.b - first_word.a == 0) return Error{-1, "Expected word"};
                            if (found_word_b_return) *found_word_b_return = first_word.b;
                            return first_word;
                        }
                        auto first_word = get_first_word(str, true);
                        auto str_cpy = str;
                        if (first_word.b - first_word.a == 0) return Error{-1, "Expected word"};
                        size_t i = first_word.b;
                        size_t next_word_id = word_id + 1;
                        size_t backup_word = next_word_id;
                        while (words[backup_word].repeat().result() == Word::RepeatType::REPEAT) ++backup_word;
                        Result<Pair<size_t, size_t>> next_word_match = Error{};
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
                        i = next_word_match.result().a;
                        if (i > 0)
                            while (is_whitespace(str[i - 1])) --i;
                        first_word.b = i;
                        if (first_word.b < first_word.a) return Error{-1, "Expected word"};
                        if (found_word_b_return) *found_word_b_return = i;
                        return first_word;
                    }
                    case Word::Type::EXPAND: {
                        if (found_word_b_return) *found_word_b_return = (size_t)str.size();
                        size_t i = 0;
                        while (is_whitespace(str[i])) ++i;
                        return Pair{i, i};
                    }
                    default: {
                        return Error{-1, "Word is not matchable"};
                    }
                };
                return {};
            }

          public:
            Result<std::vector<WordMatch>, CompilationError> match(const Source& str) const {
                if (str.empty()) return CompilationError{{}, "String is empty", CompilationError::Severity::ERROR};
                const auto valid = is_valid();
                if (valid.is_error())
                    return CompilationError{{}, valid.error().message, CompilationError::Severity::SYSTEM_ERROR};

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
                                return CompilationError{(str + pos).pos, "Single repeating word not found",
                                                        CompilationError::Severity::ERROR};
                            repeating = false;
                            ++i;
                            continue;
                        }
                        if (repeating) {
                            if (words[i].repeat().result() == Word::RepeatType::REPEAT) {
                                while (words[i].repeat().result() == Word::RepeatType::REPEAT) ++i;
                                word_match = ensure_word_match(str + pos, i);
                                if (!word_match.is_error()) continue;
                            }
                            if (i > 0) {
                                while (words[i - 1].repeat().result() == Word::RepeatType::REPEAT) {
                                    --i;
                                    if (i == 0) break;
                                }
                                word_match = ensure_word_match(str + pos, i);
                                if (!word_match.is_error()) continue;
                            }
                            return CompilationError{(str + pos).pos,
                                                    "Repeating word not found or no closer was found after repeating words",
                                                    CompilationError::Severity::ERROR};
                        }
                        if (words[i].optional().result() == Word::OptionalType::OPTIONAL) {
                            ++i;
                            continue;
                        }
                        if (words[i].optional().result() == Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE) {
                            if (words[i + 1].optional().result() != Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE)
                                return CompilationError{(str + pos).pos,
                                                        "Word should match at least one option in optional list",
                                                        CompilationError::Severity::ERROR};
                            while (words[i].optional().result() == Word::OptionalType::OPTIONAL_LIST_MANDATORY_ONE) ++i;
                            continue;
                        }
                        return CompilationError{(str + pos).pos,
                                                std::string{"Word \""} + words[i].word.substr(3) + "\" not found",
                                                CompilationError::Severity::ERROR};
                    }
                    pos = word_match.result().b;
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
            std::any user_data{};

            Extension() = default;
            Extension(const std::any& user_data) : user_data{user_data} {}

            virtual Result<std::string>
            operator()(System& system, const std::unordered_map<std::string, std::vector<std::string>>& found_words) = 0;

            virtual ~Extension() = default;
        };

        std::vector<Rule> rules{};
        std::unordered_map<std::string, Extension*> extensions{};

        template<typename T, std::enable_if_t<std::is_base_of_v<Extension, T>, bool> = true>
        void add_extension(const std::string& name, T&& ext) {
            extensions[name] = new T{std::forward<T>(ext)};
        }

        System() {
            struct ExpandCountExtension : public Extension {
                using Extension::Extension;
                Result<std::string>
                operator()(System& system,
                           const std::unordered_map<std::string, std::vector<std::string>>& found_words) override {
                    return std::to_string(std::any_cast<size_t&>(user_data)++);
                }
            };
            add_extension("EXPAND_COUNT", ExpandCountExtension{size_t{}});
        }
        System(const System& other) = default;
        System(System&& other) = default;
        System& operator=(const System& other) = default;
        System& operator=(System&& other) = default;

      private:
        Result<std::string> expand_generic(const std::string& str,
                                           const std::unordered_map<std::string, std::vector<std::string>>& expand_vars) {
            const auto expr_to_expand = get_first_word(str, true);
            if (expr_to_expand.b - expr_to_expand.a == 0) return Error{-1, "Expected expression after $"};

            if (str[expr_to_expand.a] == '(') {
                std::vector<Pair<std::string, Rule::Word::Type>> words_in_expr{};
                std::vector<Pair<size_t, size_t>> exprs_to_expand{};
                size_t max_iterations = (size_t)-1;
                for (size_t i = expr_to_expand.a + 1; i < expr_to_expand.b - 1; i++) {
                    if (str[i] == '$') {
                        ++i;
                        auto word_to_expand = get_first_word(str.substr(i), true);
                        word_to_expand = Pair{word_to_expand.a + i, word_to_expand.b + i};
                        if (str[word_to_expand.a] == '(') {
                            const auto expand_result =
                                expand_generic(str.substr(word_to_expand.a, word_to_expand.b - word_to_expand.a), expand_vars);
                            if (expand_result.is_error()) return expand_result.error();
                            words_in_expr.emplace_back(expand_result.result(), Rule::Word::Type::DIRECT);
                        }
                        else if (is_alpha(str[word_to_expand.a])) {
                            words_in_expr.emplace_back(str.substr(word_to_expand.a, word_to_expand.b - word_to_expand.a),
                                                       Rule::Word::Type::GENERIC);
                            const auto& var = expand_vars.at(words_in_expr.back().a);
                            max_iterations = std::min(max_iterations, var.size());
                        }
                        else
                            return Error{-1, "Invalid expression after $"};
                        exprs_to_expand.emplace_back(word_to_expand.a - 1, word_to_expand.b);
                        i = word_to_expand.b - 1;
                    }
                }

                if (words_in_expr.size() != exprs_to_expand.size()) return Error{-1, "Unknown internal error"};

                std::string res{};
                size_t last_word_end = 1;
                size_t current_word = 0;
                for (size_t iteration = 0; iteration < max_iterations; iteration++) {
                    for (size_t i = 0; i < words_in_expr.size(); i++) {
                        if (exprs_to_expand[i].a > last_word_end)
                            res += str.substr(last_word_end, exprs_to_expand[i].a - last_word_end);
                        last_word_end = exprs_to_expand[i].b;

                        const auto& word = words_in_expr[i];
                        switch (word.b) {
                            case Rule::Word::Type::DIRECT: {
                                res += word.a;
                                break;
                            }
                            case Rule::Word::Type::GENERIC: {
                                res += expand_vars.at(word.a)[iteration];
                                break;
                            }
                            default:
                                return Error{-1, "Unknown internal error"};
                        }
                    }
                    if (exprs_to_expand.back().b < expr_to_expand.b - 1)
                        res += str.substr(exprs_to_expand.back().b, expr_to_expand.b - exprs_to_expand.back().b - 1);
                }
                return res;
            }

            if (is_alpha(str[expr_to_expand.a])) {
                const auto var_name = str.substr(expr_to_expand.a, expr_to_expand.b - expr_to_expand.a);
                const auto ext = extensions.find(var_name);
                if (ext != extensions.end()) {
                    const auto ext_result = (*ext->second)(*this, expand_vars);
                    if (ext_result.is_error()) return ext_result.error();
                    return ext_result.result();
                }
                const auto& expand_to = expand_vars.at(var_name);
                if (expand_to.empty()) return Error{-1, "Variable not found"};
                return expand_to.front();
            }

            return Error{-1, "Invalid expression after $"};
        }

      public:
        Result<std::string, std::vector<CompilationError>> parse(Source str) {
            std::string res{};
            std::vector<CompilationError> errors{};

            for (; !str.reached_end(); ++str) {
                while (is_whitespace(*str)) ++str;

                if (*str == '"') {
                    const auto word = get_first_word(str, true);
                    res += str.source.substr(word.a + 1, word.b - word.a - 2);
                    if (word.b > str.pos.pos) str += word.b - str.pos.pos;
                    continue;
                }

                const Rule* found_rule = nullptr;
                std::vector<Rule::WordMatch> found_words{};
                float best_match_score = 0.0f;

                size_t num_errs_found = 0;
                for (const auto& rule : rules) {
                    const auto _found_words = rule.match(str);
                    if (_found_words.is_error()) {
                        errors.emplace_back(_found_words.error().pos, _found_words.error().message);
                        ++num_errs_found;
                        continue;
                    }
                    if (_found_words.result().size() > 0) {
                        float match_score = (float)(_found_words.result().back().id + 1) / (float)rule.words.size();
                        if (match_score == 1.0f &&
                            rule.words[rule.words.size() - 2].type().result() == Rule::Word::Type::DIRECT &&
                            rule.words.back().type().result() == Rule::Word::Type::EXPAND)
                            match_score = 2.0f;

                        if (match_score > best_match_score) {
                            found_rule = &rule;
                            found_words = _found_words.result();
                            best_match_score = match_score;
                        }
                        if (best_match_score == 2.0f) break;
                    }
                }

                if (best_match_score >= 1.0f) {
                    if (num_errs_found > 0) errors.resize(errors.size() - num_errs_found);
                    std::unordered_map<std::string, std::vector<std::string>> expand_vars{};
                    for (const auto& word : found_words)
                        if (found_rule->words[word.id].type().result() == Rule::Word::Type::GENERIC)
                            expand_vars[found_rule->words[word.id].word.substr(3)].emplace_back(
                                str.source.substr(word.match.a, word.match.b - word.match.a));

                    auto expand = found_rule->words.back().word.substr(3);
                    for (size_t j = 0; j < expand.size(); j++) {
                        if (expand[j] == '$') {
                            ++j;
                            auto expand_expr = get_first_word(expand.substr(j), true);
                            expand_expr = Pair{expand_expr.a + j, expand_expr.b + j};
                            const auto expand_result =
                                expand_generic(expand.substr(expand_expr.a, expand_expr.b - expand_expr.a), expand_vars);
                            if (expand_result.is_error()) {
                                errors.emplace_back(str.pos, expand_result.error().message);
                                break;
                            }
                            expand = expand.substr(0, j - 1) + expand_result.result() + expand.substr(expand_expr.b);
                            j = j - 1 + expand_result.result().size();
                        }
                    }
                    const auto parse_result = parse(expand);
                    if (parse_result.is_error())
                        errors.insert(errors.end(), parse_result.error().begin(), parse_result.error().end());
                    else
                        res += parse_result.result();
                    const auto last_word_is_expand =
                        found_rule->words.back().type().result() == Rule::Word::Type::EXPAND ? 2 : 1;
                    if (found_words[found_words.size() - last_word_is_expand].match.b > str.pos.pos)
                        str += found_words[found_words.size() - last_word_is_expand].match.b - str.pos.pos;
                    continue;
                }

                const auto word = get_first_word(str, false);
                if (word.b > str.pos.pos) {
                    errors.emplace_back(str.pos, "Unknown word: " + str.source.substr(word.a, word.b - word.a));
                    str += word.b - str.pos.pos;
                }
            }

            if (!errors.empty()) return errors;
            return res;
        }

        ~System() {
            for (auto& ext : extensions) delete ext.second;
        }
    };
} // namespace mgm
