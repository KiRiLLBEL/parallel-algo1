#pragma once

#include <concepts>
#include <cstddef>
#include <parlay/parallel.h>

namespace quicksort
{
template <typename T>
concept Sortable = std::totally_ordered<T> && std::copyable<T>;

namespace detail
{
inline constexpr std::size_t InsertionLimit = 1 << 5;
inline constexpr std::size_t ParallelLimit = 1 << 11;
inline constexpr std::size_t BlockSize = 1 << 14;

template <Sortable T> inline auto Swap(T &a, T &b) -> void
{
    const T t = a;
    a = b;
    b = t;
}

template <Sortable T> inline auto Median(const T &a, const T &b, const T &c) -> T
{
    if (a < b)
    {
        if (b < c)
        {
            return b;
        }
        if (a < c)
        {
            return c;
        }
        return a;
    }
    if (a < c)
    {
        return a;
    }
    if (b < c)
    {
        return c;
    }
    return b;
}

template <Sortable T> inline auto ChoosePivot(const T *a, std::size_t n) -> T
{
    return Median(a[0], a[n / 2], a[n - 1]);
}

template <Sortable T> inline auto InsertionSort(T *a, std::size_t n) -> void
{
    for (auto i = 1uz; i < n; ++i)
    {
        const T v = a[i];
        std::size_t j = i;
        while (j && v < a[j - 1])
        {
            a[j] = a[j - 1];
            --j;
        }
        a[j] = v;
    }
}

} // namespace detail

template <Sortable T> inline auto Sequential(T *a, std::size_t n) -> void
{
    while (n > detail::InsertionLimit)
    {
        const T p = detail::ChoosePivot(a, n);
        std::size_t left = 0, right = n;
        if (a[0] == p && a[n / 2] == p && a[n - 1] == p)
        {
            std::size_t i = 0;
            while (i < right)
            {
                if (a[i] < p)
                {
                    detail::Swap(a[left++], a[i++]);
                }
                else if (a[i] > p)
                {
                    detail::Swap(a[i], a[--right]);
                }
                else
                {
                    ++i;
                }
            }
        }
        else
        {
            std::size_t i = 0, j = n - 1;
            for (;;)
            {
                while (a[i] < p)
                {
                    ++i;
                }
                while (a[j] > p)
                {
                    --j;
                }
                if (i >= j)
                {
                    break;
                }
                detail::Swap(a[i++], a[j--]);
            }
            left = right = i;
        }
        if (left < n - right)
        {
            Sequential(a, left);
            a += right;
            n -= right;
        }
        else
        {
            Sequential(a + right, n - right);
            n = left;
        }
    }

    detail::InsertionSort(a, n);
}

namespace detail
{
struct Counts
{
    std::size_t less, equal;
};

inline auto TreeCapacity(std::size_t n) -> std::size_t
{
    constexpr std::size_t grain = ParallelLimit < BlockSize / 2 ? ParallelLimit : BlockSize / 2;
    return n <= ParallelLimit ? 0 : 4 * (n / grain);
}

template <typename T> struct Buffer
{
    T *data;

    explicit Buffer(std::size_t n) : data(new T[n])
    {
    }

    ~Buffer()
    {
        delete[] data;
    }

    Buffer(const Buffer &) = delete;
    auto operator=(const Buffer &) -> Buffer & = delete;
};

template <Sortable T>
inline auto CountTree(const T *a, std::size_t n, const T &p, Counts *tree, std::size_t node) -> void
{
    if (n <= BlockSize)
    {
        std::size_t less = 0, equal = 0;
        for (auto i = 0uz; i < n; ++i)
        {
            less += a[i] < p;
            equal += a[i] == p;
        }
        tree[node] = {less, equal};
        return;
    }
    const std::size_t mid = n / 2;

    parlay::par_do([&]() -> void { CountTree(a, mid, p, tree, 2 * node); },
                   [&]() -> void { CountTree(a + mid, n - mid, p, tree, 2 * node + 1); });

    tree[node] = {tree[2 * node].less + tree[2 * node + 1].less,
                  tree[2 * node].equal + tree[2 * node + 1].equal};
}

template <Sortable T>
inline auto Scatter(const T *a, T *out, std::size_t n, const T &p, const Counts *tree,
                    std::size_t node, std::size_t less, std::size_t equal, std::size_t greater)
    -> void
{
    if (n <= BlockSize)
    {
        for (auto i = 0uz; i < n; ++i)
        {
            const T &v = a[i];
            if (v == p)
            {
                out[equal++] = v;
                continue;
            }
            const bool small = v < p;
            out[small ? less : greater] = v;
            less += small;
            greater += !small;
        }
        return;
    }

    const std::size_t mid = n / 2;
    const auto l = tree[2 * node];

    parlay::par_do([&]() -> void { Scatter(a, out, mid, p, tree, 2 * node, less, equal, greater); },
                   [&]() -> void {
                       Scatter(a + mid, out, n - mid, p, tree, 2 * node + 1, less + l.less,
                               equal + l.equal, greater + mid - l.less - l.equal);
                   });
}

template <Sortable T> inline auto Copy(const T *src, T *out, std::size_t n) -> void
{
    if (n <= BlockSize)
    {
        for (auto i = 0uz; i < n; ++i)
        {
            out[i] = src[i];
        }
    }
    else
    {
        const std::size_t mid = n / 2;

        parlay::par_do([&]() -> void { Copy(src, out, mid); },
                       [&]() -> void { Copy(src + mid, out + mid, n - mid); });
    }
}

template <Sortable T>
inline auto SortParallelParts(T *src, T *scratch, T *answer, std::size_t n, Counts *tree) -> void
{
    if (n <= ParallelLimit)
    {
        if (src != answer)
        {
            for (auto i = 0uz; i < n; ++i)
            {
                answer[i] = src[i];
            }
        }
        Sequential(answer, n);
        return;
    }

    const T p = ChoosePivot(src, n);
    CountTree(src, n, p, tree, 1);
    const auto total = tree[1];
    const std::size_t greater = total.less + total.equal;

    if (total.less == 0 || greater == n)
    {
        if (src != answer)
        {
            Copy(src, answer, n);
        }
        Sequential(answer, n);
        return;
    }

    Scatter(src, scratch, n, p, tree, 1, 0, total.less, greater);
    if (scratch != answer)
    {
        Copy(scratch + total.less, answer + total.less, total.equal);
    }

    const auto rightTreeOffset = TreeCapacity(total.less);

    parlay::par_do([&]() -> void { SortParallelParts(scratch, src, answer, total.less, tree); },
                   [&]() -> void {
                       SortParallelParts(scratch + greater, src + greater, answer + greater,
                                         n - greater, tree + rightTreeOffset);
                   });
}
} // namespace detail

template <Sortable T>
    requires std::default_initializable<T>
inline auto Parallel(T *a, std::size_t n) -> void
{
    if (n <= detail::ParallelLimit)
    {
        return Sequential(a, n);
    }

    detail::Buffer<T> scratch(n);
    detail::Buffer<detail::Counts> tree(detail::TreeCapacity(n));

    detail::SortParallelParts(a, scratch.data, a, n, tree.data);
}
} // namespace quicksort
