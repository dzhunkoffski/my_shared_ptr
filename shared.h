#pragma once

#include <cstddef>  // std::nullptr_t

class ControlBlockBase {
public:
    size_t ref_cnt = 1;

    virtual ~ControlBlockBase() = default;
};

template <typename Y>
class ControlBlockPtr : public ControlBlockBase {
public:
    Y* ptr_;
    ControlBlockPtr(Y* ptr = nullptr) : ptr_(ptr) {
    }

    ~ControlBlockPtr() {
        if (ptr_) {
            delete ptr_;
        }
    }
};

template <typename Y>
class ControlBlockHolder : public ControlBlockBase {
public:
    typename std::aligned_storage<sizeof(Y), alignof(Y)>::type storage_;

    template <typename... Args>
    ControlBlockHolder(Args&&... args) {
        new (&storage_) Y(std::forward<Args>(args)...);
    }
    ~ControlBlockHolder() {
        reinterpret_cast<Y*>(&storage_)->~Y();
    }

    Y* GetRawPointer() {
        return reinterpret_cast<Y*>(&storage_);
    }
};

// https://en.cppreference.com/w/cpp/memory/shared_ptr
template <typename T>
class SharedPtr {
private:
    T* data_{};
    ControlBlockBase* control_block_;

    template <typename Y>
    friend class SharedPtr;

    template <typename Y, typename... Args>
    friend SharedPtr<Y> MakeShared(Args&&... args);

public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Constructors

    SharedPtr() {
        data_ = nullptr;
        control_block_ = nullptr;
    }
    SharedPtr(std::nullptr_t) {
        data_ = nullptr;
        control_block_ = nullptr;
    }

    template <typename Y>
    explicit SharedPtr(Y* ptr) : data_(ptr), control_block_(new ControlBlockPtr<Y>(ptr)) {
    }

    SharedPtr(const SharedPtr& other) : data_(other.data_), control_block_(other.control_block_) {
        if (control_block_) {
            ++control_block_->ref_cnt;
        }
    }
    SharedPtr(SharedPtr&& other) : data_(other.data_), control_block_(other.control_block_) {
        other.data_ = nullptr;
        other.control_block_ = nullptr;
    }

    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other) : data_(other.data_), control_block_(other.control_block_) {
        if (control_block_) {
            ++control_block_->ref_cnt;
        }
    }
    template <typename Y>
    SharedPtr(SharedPtr<Y>&& other) : data_(other.data_), control_block_(other.control_block_) {
        other.data_ = nullptr;
        other.control_block_ = nullptr;
    }

    // Aliasing constructor
    // #8 from https://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, T* ptr)
            : data_(ptr), control_block_(other.control_block_) {
        ++control_block_->ref_cnt;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // `operator=`-s

    SharedPtr& operator=(const SharedPtr& other) {
        this->Reset();

        data_ = other.data_;
        control_block_ = other.control_block_;
        if (control_block_) {
            ++control_block_->ref_cnt;
        }

        return *this;
    }
    SharedPtr& operator=(SharedPtr&& other) {
        this->Reset();

        data_ = other.Get();
        control_block_ = other.control_block_;

        other.data_ = nullptr;
        other.control_block_ = nullptr;

        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Destructor

    ~SharedPtr() {
        if (control_block_) {
            if (control_block_->ref_cnt == 1) {
                delete control_block_;
            } else {
                --control_block_->ref_cnt;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Modifiers

    void Reset() {
        if (control_block_) {
            if (control_block_->ref_cnt == 1) {
                delete control_block_;
            } else {
                --control_block_->ref_cnt;
            }
        }

        data_ = nullptr;
        control_block_ = nullptr;
    }

    template <typename Y>
    void Reset(Y* ptr) {
        if (control_block_) {
            if (control_block_->ref_cnt == 1) {
                delete control_block_;
            } else {
                --control_block_->ref_cnt;
            }
        }

        data_ = ptr;
        control_block_ = new ControlBlockPtr<Y>(ptr);
    }
    void Swap(SharedPtr& other) {
        auto tmp = *this;
        *this = other;
        other = tmp;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Observers

    T* Get() const {
        return data_;
    }
    T& operator*() const {
        return *data_;
    }
    T* operator->() const {
        return data_;
    }
    size_t UseCount() const {
        if (control_block_) {
            return control_block_->ref_cnt;
        }

        return 0;
    }
    explicit operator bool() const {
        if (data_) {
            return true;
        } else {
            return false;
        }
    }
};

template <typename T, typename U>
inline bool operator==(const SharedPtr<T>& left, const SharedPtr<U>& right);

// Allocate memory only once
template <typename Y, typename... Args>
SharedPtr<Y> MakeShared(Args&&... args) {
    SharedPtr<Y> shared_ptr;
    auto block = new ControlBlockHolder<Y>(std::forward<Args>(args)...);
    shared_ptr.control_block_ = block;
    shared_ptr.data_ = block->GetRawPointer();
    return shared_ptr;
}
