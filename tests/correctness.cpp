#include "quicksort.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <compare>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace
{
auto MakeRandom() -> std::mt19937
{
    const auto seed = testing::UnitTest::GetInstance()->random_seed();
    std::cout << "[  RANDOM  ] Reproduce with --gtest_random_seed=" << seed << '\n';
    return std::mt19937(static_cast<std::mt19937::result_type>(seed));
}

struct Record
{
    int key = 0;
    int payload = 0;

    auto operator<=>(const Record &other) const -> std::strong_ordering
    {
        return key <=> other.key;
    }

    auto operator==(const Record &other) const -> bool
    {
        return key == other.key;
    }
};

class QuickSortTest : public testing::TestWithParam<bool>
{
  protected:
    template <quicksort::Sortable T> auto Sort(std::vector<T> &data) -> void
    {
        if (GetParam())
        {
            quicksort::Parallel(data.data(), data.size());
        }
        else
        {
            quicksort::Sequential(data.data(), data.size());
        }
    }

    template <quicksort::Sortable T> auto CheckTyped(const std::vector<T> &input) -> void
    {
        SCOPED_TRACE(testing::Message() << "size=" << input.size());
        auto expected = input;
        auto actual = input;
        std::sort(expected.begin(), expected.end());
        Sort(actual);
        EXPECT_EQ(actual, expected);
    }

    auto Check(const std::vector<std::int32_t> &input) -> void
    {
        CheckTyped(input);
    }
};

TEST_P(QuickSortTest, EmptyArray)
{
    Check({});
}

TEST_P(QuickSortTest, SingleElement)
{
    Check({42});
}

TEST_P(QuickSortTest, ReversedPair)
{
    Check({2, 1});
}

TEST_P(QuickSortTest, IntegerExtremes)
{
    Check({std::numeric_limits<std::int32_t>::max(), 0, -1,
           std::numeric_limits<std::int32_t>::min(), 1, 0});
}

TEST_P(QuickSortTest, ExhaustiveSmallArrays)
{
    for (std::size_t size = 0, count = 1; size <= 8; ++size, count *= 3)
    {
        for (std::size_t mask = 0; mask < count; ++mask)
        {
            SCOPED_TRACE(testing::Message() << "mask=" << mask);
            std::vector<std::int32_t> data(size);
            auto digits = mask;
            for (auto &value : data)
            {
                value = static_cast<int>(digits % 3) - 1;
                digits /= 3;
            }
            Check(data);
        }
    }
}

TEST_P(QuickSortTest, PatternsAtLimits)
{
    auto random = MakeRandom();
    const std::size_t sizes[] = {quicksort::detail::InsertionLimit - 1,
                                 quicksort::detail::InsertionLimit,
                                 quicksort::detail::InsertionLimit + 1,
                                 31,
                                 32,
                                 33,
                                 127,
                                 1024,
                                 quicksort::detail::BlockSize - 1,
                                 quicksort::detail::BlockSize,
                                 quicksort::detail::BlockSize + 1,
                                 quicksort::detail::ParallelLimit - 1,
                                 quicksort::detail::ParallelLimit,
                                 quicksort::detail::ParallelLimit + 1,
                                 2 * quicksort::detail::ParallelLimit + 17,
                                 1000003};
    for (auto size : sizes)
    {
        for (int pattern = 0; pattern < 10; ++pattern)
        {
            SCOPED_TRACE(testing::Message() << "pattern=" << pattern);
            std::vector<std::int32_t> data(size);
            for (std::size_t i = 0; i < size; ++i)
            {
                switch (pattern)
                {
                case 0:
                    data[i] = static_cast<std::int32_t>(static_cast<std::int64_t>(random()) +
                                                        std::numeric_limits<std::int32_t>::min());
                    break;
                case 1:
                    data[i] = static_cast<std::int32_t>(i);
                    break;
                case 2:
                    data[i] = static_cast<std::int32_t>(size - i);
                    break;
                case 3:
                    data[i] = 7;
                    break;
                case 4:
                    data[i] = static_cast<std::int32_t>(random() % 2);
                    break;
                case 5:
                    data[i] = static_cast<int>(random() % 17) - 8;
                    break;
                case 6:
                    data[i] = static_cast<std::int32_t>(i <= size / 2 ? i : size - i);
                    break;
                case 7:
                    data[i] = static_cast<std::int32_t>(i % 97);
                    break;
                case 8:
                    data[i] = i % 2 ? std::numeric_limits<std::int32_t>::min()
                                    : std::numeric_limits<std::int32_t>::max();
                    break;
                case 9:
                    data[i] = i == size / 2 ? -1 : 0;
                    break;
                }
            }
            Check(data);
        }
    }
}

TEST_P(QuickSortTest, RandomizedArrays)
{
    auto random = MakeRandom();
    for (int trial = 0; trial < 120; ++trial)
    {
        SCOPED_TRACE(testing::Message() << "trial=" << trial);
        const std::size_t size =
            trial < 100 ? random() % 10000 : quicksort::detail::ParallelLimit + random() % 500000;
        std::vector<std::int32_t> data(size);
        for (auto &value : data)
        {
            value = static_cast<int>(random() % (trial % 3 ? 1000000 : 7)) - 3;
        }
        Check(data);
    }
}

TEST_P(QuickSortTest, OtherOrderedTypes)
{
    for (const std::size_t size :
         {std::size_t{0}, std::size_t{1}, quicksort::detail::InsertionLimit - 1,
          quicksort::detail::InsertionLimit, quicksort::detail::InsertionLimit + 1,
          2 * quicksort::detail::ParallelLimit + 1})
    {
        std::vector<std::int64_t> integers(size);
        std::vector<double> fractions(size);
        std::vector<std::string> strings(size);
        for (std::size_t i = 0; i < size; ++i)
        {
            const auto value = static_cast<std::int64_t>((i * 97) % 257) - 128;
            integers[i] = value * 10000000000LL;
            fractions[i] = static_cast<double>(value) / 4;
            strings[i] = "string beyond the small-string buffer: " + std::to_string(value);
        }
        CheckTyped(integers);
        CheckTyped(fractions);
        CheckTyped(strings);
    }
}

TEST_P(QuickSortTest, PreservesPayloadForEqualKeys)
{
    for (const int keys : {1, 17})
    {
        std::vector<Record> data(2 * quicksort::detail::ParallelLimit + 1);
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            data[i] = {static_cast<int>(i) % keys, static_cast<int>(i)};
        }
        Sort(data);
        EXPECT_TRUE(std::is_sorted(data.begin(), data.end()));
        std::vector<bool> seen(data.size());
        for (const auto &record : data)
        {
            ASSERT_GE(record.payload, 0);
            const auto index = static_cast<std::size_t>(record.payload);
            ASSERT_LT(index, seen.size());
            EXPECT_FALSE(seen[index]);
            seen[index] = true;
            EXPECT_EQ(record.key, record.payload % keys);
        }
    }
}

TEST(TreeWorkspace, CountTreeStaysInsideSlice)
{
    constexpr auto sentinel = std::numeric_limits<std::size_t>::max();
    for (const std::size_t size :
         {quicksort::detail::ParallelLimit + 1, 2 * quicksort::detail::ParallelLimit - 1,
          quicksort::detail::BlockSize - 1, quicksort::detail::BlockSize + 1,
          2 * quicksort::detail::BlockSize + 1, 4 * quicksort::detail::BlockSize + 1})
    {
        SCOPED_TRACE(testing::Message() << "size=" << size);
        std::vector<int> data(size);
        quicksort::detail::Counts expected{0, 0};
        for (std::size_t i = 0; i < size; ++i)
        {
            data[i] = static_cast<int>(i % 19);
            expected.less += data[i] < 9;
            expected.equal += data[i] == 9;
        }
        std::vector<quicksort::detail::Counts> storage(quicksort::detail::TreeCapacity(size) + 2,
                                                       {sentinel, sentinel});
        quicksort::detail::CountTree(data.data(), size, 9, storage.data() + 1, 1);
        EXPECT_EQ(storage.front().less, sentinel);
        EXPECT_EQ(storage.front().equal, sentinel);
        EXPECT_EQ(storage.back().less, sentinel);
        EXPECT_EQ(storage.back().equal, sentinel);
        EXPECT_EQ(storage[2].less, expected.less);
        EXPECT_EQ(storage[2].equal, expected.equal);
    }
}

INSTANTIATE_TEST_SUITE_P(Algorithms, QuickSortTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &parameter) -> const char * {
                             return parameter.param ? "Par" : "Seq";
                         });
} // namespace
