#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
    {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
    {
        std::swap(size_, other.size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector copy_vector(rhs);
                Swap(copy_vector);
            }
            else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                size_t i = 0;
                size_t end_size = size_ > rhs.size_ ? rhs.size_ : size_;
                for (; i < end_size; ++i) {
                    data_[i] = rhs.data_[i];
                }
                if (size_ > rhs.size_) {
                    std::destroy_n(data_ + end_size, size_ - end_size);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_ + end_size, rhs.size_ - end_size, data_ + end_size);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            std::swap(size_, rhs.size_);
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (size_ > new_size) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        Insert(end(), value);
    }

    void PushBack(T&& value) {
        Insert(end(), std::move(value));
    }

    void PopBack() {
        Erase(end());
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(end(), std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t dist = pos - begin();
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + dist) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), dist, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_at(new_data + dist);
                }
                try {
                    std::uninitialized_move_n(data_ + dist, size_ - dist, new_data + dist + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), dist);
                }
            }
            else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), dist, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_at(new_data + dist + 1);
                }
                try {
                    std::uninitialized_copy_n(data_ + dist, size_ - dist, new_data + dist + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), dist + 1);
                }
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            if (pos != end())
            {
                T temp_value(std::forward<Args>(args)...);
                if constexpr (!std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>) {
                    std::uninitialized_move_n(data_ + size_ - 1, 1, data_ + size_);
                }
                else {
                    std::uninitialized_copy_n(data_ + size_ - 1, 1, data_ + size_);
                }
                std::move_backward(data_ + dist, data_ + size_ - 1, data_ + size_);
                data_[dist] = std::move(temp_value);
            }
            else {
                new (data_ + dist) T(std::forward<Args>(args)...);
            }
        }
        ++size_;
        return data_ + dist;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        if (pos != end())
        {
            std::move(data_ + (pos - begin()) + 1, end(), data_ + (pos - begin()));
        }
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
        return data_ + (pos - begin());
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

};