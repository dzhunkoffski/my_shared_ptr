#pragma once

#include <atomic>
#include <cassert>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ASAN_ENABLED
#include <sanitizer/allocator_interface.h>
#endif
#endif

std::atomic<size_t> allocations_count{0}, deallocations_count{0};

void MallocHook(const volatile void*, size_t) {
    allocations_count.fetch_add(1);
}

void FreeHook(const volatile void*) {
    deallocations_count.fetch_add(1);
}

#ifdef ASAN_ENABLED
[[maybe_unused]] const auto kInit = [] {
    int res = __sanitizer_install_malloc_and_free_hooks(MallocHook, FreeHook);
    if (res == 0) {
        throw std::runtime_error{"Failed to install ASan allocator hooks"};  // just terminate
    }
    return 0;
}();
#else
void* operator new(size_t size) {
    void* p = malloc(size);
    MallocHook(p, size);
    return p;
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    void* p = malloc(size);
    MallocHook(p, size);
    return p;
}

void operator delete(void* p) noexcept {
    FreeHook(p);
    free(p);
}

void operator delete(void* p, size_t) noexcept {
    FreeHook(p);
    free(p);
}
#endif

#define EXPECT_ZERO_ALLOCATIONS(X)                  \
    do {                                            \
        auto __xxx = allocations_count.load();      \
        X;                                          \
        assert(allocations_count.load() == __xxx); \
    } while (0)

#define EXPECT_ONE_ALLOCATION(X)                        \
    do {                                                \
        auto __xxx = allocations_count.load();          \
        X;                                              \
        assert(allocations_count.load() == __xxx + 1); \
    } while (0)

#define EXPECT_NO_MORE_THAN_ONE_ALLOCATION(X)           \
    do {                                                \
        auto __xxx = allocations_count.load();          \
        X;                                              \
        assert(allocations_count.load() <= __xxx + 1); \
    } while (0)
