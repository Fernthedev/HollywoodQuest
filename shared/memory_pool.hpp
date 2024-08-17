#include <memory>
#include <vector>

namespace Hollywood {

// holds data

// holds data
template <typename T>
struct MemoryPoolHandle {
    bool inUse = false;
    T data;

    MemoryPoolHandle() = default;
    MemoryPoolHandle(MemoryPoolHandle&&) = default;
    MemoryPoolHandle(MemoryPoolHandle const&) = delete;

    MemoryPoolHandle(T&& t) : data(std::forward<T>(t)) {}
    MemoryPoolHandle(T const& t) : data(t) {}

    ~MemoryPoolHandle() = default;
};

// Synchronous Arena based allocation. Manages everything
template <typename T>
struct MemoryPool {

    MemoryPool() = default;

    // shared ptr default constructor is null
    // so we give the vector a value here
    MemoryPool(std::size_t initial) : handles(initial, makeHandle()) {}

    using Handle = std::shared_ptr<MemoryPoolHandle<T>>;
    // this is so cursed. Basically make a shared ptr of a shared ptr
    // so we can have a custom delete function
    // essentially working as shared RAII
    using Reference = std::shared_ptr<Handle>;

    // resizing is safe, we use shared ptrs
    std::vector<Handle> handles;

    template <typename... TArgs>
    Reference alloc(TArgs&&... args) {

        // look for existing handle
        for (auto& handle : handles) {
            if (handle->inUse) {
                continue;
            }

            handle->data = T(std::forward<TArgs>(args)...);
            handle->inUse = true;
            return makeReference(handle);
        }

        // make new one
        auto handle = makeHandle(std::forward<TArgs>(args)...);
        handle->inUse = true;

        handles.emplace_back(handle);

        return makeReference(handle);
    }

   private:
    template <typename... TArgs>
    constexpr Handle makeHandle(TArgs&&... args) {
        return std::make_shared<MemoryPoolHandle<T>>(std::forward<TArgs>(args)...);
    }
    constexpr Reference makeReference(Handle const& handle) { return std::shared_ptr<Handle>(new Handle(handle), &MemoryPool<T>::dealloc); }

    static void dealloc(Handle* reference) {
        reference->get()->inUse = false;
        delete reference;
    }
};
}