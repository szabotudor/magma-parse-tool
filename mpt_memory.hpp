#pragma once
#include <vector>


namespace mgm {
    template<bool use_stack_allocator = false, bool self_managing_references = false>
    class MemoryBlock {
        enum ErrorCode {
            UNKNOWN_ERROR = 0,

            NOT_ENOUGH_MEMORY = 1,
            INVALID_ADDRESS = 2,
            INVALID_SIZE = 3,
            INVALID_REFERENCE = 4,
            INVALID_MEMORY_BLOCK = 5,
            ADDRESS_IN_USE = 6,
            UNNECESSARY_CALL = 7
        };
        template<size_t _max_sections = 15>
        struct Header {
            static constexpr size_t max_sections = _max_sections;
            struct Section {
                size_t addr{}, size{};
            };

            size_t num_sections = 0;
            size_t next = 0;
            Section sections[max_sections];

            Header() { memset(sections, 0, sizeof(sections)); }
        };
        struct StackHeader {
            size_t addr{};
        };

        template<typename T>
        class BufferReference {
            friend class MemoryBlock;
            MemoryBlock* mb;
            size_t pos;

            BufferReference(MemoryBlock* mb, size_t pos) : mb{mb}, pos{pos} {
                ++mb->refs_count;
                if constexpr (self_managing_references) {
                    for (auto& ref : mb->refs_counts) {
                        if (ref.num == 0) {
                            ref.num = 1;
                            ref.addr = pos;
                            pos = &ref - mb->refs_counts.get_data();
                            return;
                        }
                    }
                    mb->refs_counts.emplace_back(ManagedReferenceData{1, pos}).num = 1;
                    this->pos = mb->refs_counts.get_size() - 1;
                }
            }

            void copy(const BufferReference& br) {
                mb = br.mb;
                pos = br.pos;
                if constexpr (self_managing_references)
                    ++mb->refs_counts[pos].num;
                ++mb->refs_count;
            }
            void move(BufferReference& br) {
                mb = br.mb;
                pos = br.pos;
                br.invalidate_without_reduce_num();
            }

            public:
            BufferReference(const BufferReference& br) {
                copy(br);
            }
            BufferReference(BufferReference&& br) {
                move(br);
            }
            Result<BufferReference&> operator=(const BufferReference& br) {
                if (this == &br)
                    return *this;

                copy(br);
                return *this;
            }
            BufferReference& operator=(BufferReference&& br) {
                if (this == &br)
                    return *this;

                move(br);
                return *this;
            }

            private:
            Result<T*> get(size_t p) {
                if constexpr (self_managing_references)
                    p = mb->refs_counts[pos].addr + p;
                else
                    p = pos + p;

                if constexpr (use_stack_allocator)
                    p = p - (mb->max_alloc - mb->data_size);

                if (p > mb->data_size - sizeof(T))
                    return Error{ErrorCode::INVALID_REFERENCE, "Invalid reference"};
                return (T*)&mb->data[p];
            }

            public:
            T& operator*() { return *get(0).result(); }
            T* operator->() { return get(0).result(); }
            template<bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<!_use_stack_allocator, bool> = true>
            T& operator[](const size_t i) { return *get(i * sizeof(T)).result(); }

            private:
            void invalidate_without_reduce_num() {
                mb = nullptr;
                pos = 0xffffffffffffffff;
            }

            public:
            Result<bool> invalidate() {
                if (!valid())
                    return Error{ErrorCode::UNNECESSARY_CALL, "Reference already invalidated"};
                if constexpr (self_managing_references) {
                    if (mb->refs_counts[pos].num == 0)
                        return Error{ErrorCode::UNKNOWN_ERROR, "Internal error"};
                    --mb->refs_counts[pos].num;
                    if (mb->refs_counts[pos].num == 0) {
                        mb->free_bytes(mb->refs_counts[pos].addr);
                        mb->refs_counts[pos].addr = 0;
                    }
                }
                if (mb)
                    --mb->refs_count;
                invalidate_without_reduce_num();
                return true;
            }
            bool valid() const { return mb != nullptr; }

            size_t get_pos() const {
                if constexpr (self_managing_references)
                    return mb->refs_counts[pos].addr;
                else
                    return pos;
            }

            ~BufferReference() {
                invalidate();
            }
        };
        template<typename T>
        friend class BufferReference;

        Result<size_t> assure_space(const size_t end) {
            if (end <= data_size)
                return data_size;
            if (max_alloc > 0 && end > max_alloc)
                return Error{ErrorCode::NOT_ENOUGH_MEMORY, "Allocation would exceed maximum allowed size"};

            resize_data(end);
            return data_size;
        }

        template<bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<!_use_stack_allocator, bool> = true>
        Result<size_t> allocate_bytes(const size_t size, size_t offset = 0) {
            const auto init = assure_space(sizeof(Header<>) + sizeof(size_t) + size);
            if (init.is_error())
                return init.error();

            Header<>* header = (Header<>*)&data[offset];
            while (header->next && header->num_sections == Header<>::max_sections) {
                offset = header->next;
                header = (Header<>*)&data[offset];
            }
            if (header->num_sections == Header<>::max_sections) {
                const auto& s = header->sections[Header<>::max_sections - 1];
                header->next = s.addr + s.size;
                auto& new_header = *(Header<>*)&data[header->next];
                new (&new_header) Header<>{};
                return allocate_bytes(size, header->next);
            }

            if (header->num_sections == 0) {
                const auto reserved =  assure_space(offset + sizeof(Header<>) + sizeof(size_t) + size);
                if (reserved.is_error())
                    return reserved.error();

                header->sections[0] = {offset + sizeof(Header<>) + sizeof(size_t), size};
                header->num_sections = 1;
                auto& res = *(size_t*)&data[offset + sizeof(Header<>)];
                res = 0;
                return offset + sizeof(Header<>) + sizeof(size_t);
            }

            for (size_t i = 0; i < header->num_sections; i++) {
                const auto& s = header->sections[i];
                const auto& next = header->sections[i + 1];
                if (!next.addr)
                    continue;
                const auto free = next.addr - (s.addr + s.size);
                if (free >= size + sizeof(size_t) * 2) {
                    const auto addr = s.addr + s.size;
                    for (size_t j = header->num_sections; j > i; j--)
                        header->sections[j] = header->sections[j - 1];
                    header->sections[i + 1] = {offset + addr + sizeof(size_t), size};
                    auto& res = *(size_t*)&data[offset + addr];
                    res = i + 1;
                    return offset + addr;
                }
            }

            const auto& prev = header->sections[header->num_sections - 1];
            const auto reserved = assure_space(prev.addr + prev.size + sizeof(size_t) + size);
            if (reserved.is_error())
                return reserved.error();
            auto& s = header->sections[header->num_sections];
            s.addr = header->sections[header->num_sections - 1].addr + header->sections[header->num_sections - 1].size + sizeof(size_t);
            s.size = size;
            auto& res = *(size_t*)&data[s.addr - sizeof(size_t)];
            res = header->num_sections++;
            return offset + s.addr;
        }
        template<bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<_use_stack_allocator, bool> = true>
        Result<size_t> allocate_bytes(const size_t size) {
            StackHeader* header = (StackHeader*)&data[0];
            if (size > header->addr)
                return Error{ErrorCode::NOT_ENOUGH_MEMORY, "Allocation would exceed maximum allowed size"};

            header->addr -= size;
            return header->addr;
        }

        template<bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<!_use_stack_allocator, bool> = true>
        Result<bool> free_bytes(const size_t addr) {
            if (addr >= data_size - sizeof(size_t))
                return Error{ErrorCode::INVALID_ADDRESS, "Address out of range"};

            Header<>* header = (Header<>*)&data[0];
            do {
                auto& section = *(size_t*)&data[addr - sizeof(size_t)];
                if (section < header->num_sections) {
                    if (header->sections[section].addr != addr)
                        continue;
                    for (size_t i = section; i < header->num_sections - 1; i++)
                        header->sections[i] = header->sections[i + 1];
                    --header->num_sections;
                    header->sections[header->num_sections] = {};
                    return true;
                }
                header = (Header<>*)&data[header->next];
            } while (header->next);
            return Error{ErrorCode::INVALID_ADDRESS, "Invalid address"};
        }
        template<bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<_use_stack_allocator, bool> = true>
        Result<bool> free_bytes(const size_t size) {
            StackHeader* header = (StackHeader*)&data[0];
            if (size > max_alloc - header->addr)
                return Error{ErrorCode::INVALID_SIZE, "Invalid size"};

            header->addr += size;
            return true;
        }

        struct ManagedReferenceData { size_t num{}, addr{}; };
        std::vector<ManagedReferenceData> refs_counts{};
        size_t max_alloc = 0;
        size_t refs_count = 0;

        public:
        uint8_t* data = nullptr;
        size_t data_size = 0;
        void resize_data(const size_t size) {
            uint8_t* aux = std::allocator<uint8_t>{}.allocate(size);
            memcpy(aux, data, std::min(size, data_size));
            std::allocator<uint8_t>{}.deallocate(data, data_size);
            data = aux;
            data_size = size;
        }

        bool valid() const { return max_alloc != 0; }

        MemoryBlock(const size_t max_alloc = 134217728/*128 MB*/, const size_t initial_alloc = 0) : max_alloc{max_alloc} {
            if (initial_alloc > max_alloc) {
                this->max_alloc = 0;
                return;
            }

            static constexpr size_t min_alloc = sizeof(Header<>) + sizeof(size_t) * 2;
            if (max_alloc < min_alloc) {
                this->max_alloc = 0;
                return;
            }
            resize_data(std::max(initial_alloc, min_alloc));

            if constexpr (use_stack_allocator) {
                StackHeader* header = (StackHeader*)&data[0];
                header->addr = max_alloc;
            } else {
                Header<>* header = (Header<>*)&data[0];
                new (header) Header<>{};
            }
        }

        template<typename T, bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<!_use_stack_allocator, bool> = true>
        Result<BufferReference<T>> allocate(const size_t num = 1) {
            if (max_alloc == 0)
                return Error{ErrorCode::NOT_ENOUGH_MEMORY, "MemoryBlock not properly initialized"};
            auto res = allocate_bytes(sizeof(T) * num);
            if (res.is_error())
                return res.error();
            return BufferReference<T>{this, res.result()};
        }
        template<typename T, bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<_use_stack_allocator, bool> = true>
        Result<BufferReference<T>> allocate() {
            if (max_alloc == 0)
                return Error{ErrorCode::NOT_ENOUGH_MEMORY, "MemoryBlock not properly initialized"};
            auto res = allocate_bytes(sizeof(T));
            if (res.is_error())
                return res.error();
            return BufferReference<T>{this, res.result()};
        }
        template<typename T, typename... Ts, bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<_use_stack_allocator, bool> = true>
        Result<BufferReference<T>> push(Ts&&... args) {
            const auto res = allocate_bytes(sizeof(T));
            if (res.is_error())
                return res.error();
            const auto p = res.result();
            new (&data[p - (max_alloc - data_size)]) T{std::forward<Ts>(args)...};
            return BufferReference<T>{this, res.result()};
        }

        Result<bool> free(const size_t addr) {
            if constexpr (self_managing_references) {
                for (auto& ref : refs_counts) {
                    if (ref.addr == addr && ref.num > 0) {
                        return Error{ErrorCode::ADDRESS_IN_USE, "Address in use by reference(s)"};
                    }
                }
            }
            const auto res =  free_bytes(addr);
            if (res.is_error())
                return res.error();
            return true;
        }
        template<typename T, bool _use_stack_allocator = use_stack_allocator, std::enable_if_t<!_use_stack_allocator, bool> = true>
        Result<bool> pop() {
            const auto res = free_bytes(sizeof(T));
            if (res.is_error())
                return res.error();
            return true;
        }

        ~MemoryBlock() {
            if (refs_count > 0)
                std::cerr << "MemoryBlock has " << refs_count << " references left" << std::endl;
        }
    };
}
