#include "quicksort.hpp"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using Value = std::int32_t;
using Seed = std::uint32_t;

inline constexpr unsigned SequentialWorkers = 1;
inline constexpr unsigned DefaultParallelWorkers = 4;
inline constexpr benchmark::IterationCount SortsPerRepetition = 1;
inline constexpr int Repetitions = 5;
inline constexpr int SizeArgument = 0;
inline constexpr std::int64_t InputSizes[] = {10'000, 1'000'000, 100'000'000};

class RunsAndMedianReporter : public benchmark::ConsoleReporter
{
  public:
    RunsAndMedianReporter() : ConsoleReporter(OO_None)
    {
    }

  protected:
    auto PrintRunData(const Run &run) -> void override
    {
        if (run.run_type == Run::RT_Iteration || run.aggregate_name == "median")
        {
            ConsoleReporter::PrintRunData(run);
        }
    }
};

auto MakeSeed() -> Seed
{
    if (const char *value = std::getenv("QUICKSORT_BENCHMARK_SEED"))
    {
        const std::string_view text(value);
        Seed seed{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seed);
        if (error != std::errc{} || end != text.data() + text.size())
        {
            throw std::invalid_argument(
                "QUICKSORT_BENCHMARK_SEED must be an unsigned 32-bit integer");
        }
        return seed;
    }
    return std::random_device{}();
}

auto ReadParallelWorkers() -> unsigned
{
    const char *value = std::getenv("PARLAY_NUM_THREADS");
    if (value == nullptr)
    {
        return DefaultParallelWorkers;
    }
    const std::string_view text(value);
    unsigned workers{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), workers);
    if (error != std::errc{} || end != text.data() + text.size() || workers == 0)
    {
        throw std::invalid_argument("PARLAY_NUM_THREADS must be a positive unsigned integer");
    }
    return workers;
}

auto RunSort(benchmark::State &state, bool parallel, unsigned parallelWorkers, Seed seed,
             bool &failed) -> void
{
    const auto size = static_cast<std::size_t>(state.range(SizeArgument));
    std::mt19937 random(seed);
    std::uniform_int_distribution<Value> distribution(std::numeric_limits<Value>::lowest(),
                                                      std::numeric_limits<Value>::max());
    std::vector<Value> data(size);
    std::ranges::generate(data, [&]() -> Value { return distribution(random); });
    auto expected = data;
    std::ranges::sort(expected);

    auto sort = parallel ? quicksort::Parallel<Value> : quicksort::Sequential<Value>;
    std::size_t workers = SequentialWorkers;
    auto measure = [&]() -> void {
        for (auto iteration : state)
        {
            (void)iteration;
            sort(data.data(), size);
            benchmark::DoNotOptimize(data.data());
            benchmark::ClobberMemory();
        }
    };
    if (parallel)
    {
        parlay::execute_with_scheduler(parallelWorkers, [&]() -> void {
            workers = parlay::num_workers();
            measure();
        });
    }
    else
    {
        measure();
    }

    const bool verified = std::ranges::equal(data, expected);
    if (!verified)
    {
        failed = true;
        state.SkipWithError("result differs from std::ranges::sort");
        return;
    }
    state.counters["workers"] = static_cast<double>(workers);
    state.counters["verified"] = static_cast<double>(verified);
    state.SetItemsProcessed(static_cast<std::int64_t>(size));
}

auto RegisterSort(const char *name, bool parallel, unsigned parallelWorkers, Seed seed,
                  bool &failed) -> void
{
    auto *series = benchmark::RegisterBenchmark(
        name, [parallel, parallelWorkers, seed, &failed](benchmark::State &state) -> void {
            RunSort(state, parallel, parallelWorkers, seed, failed);
        });
    for (const auto size : InputSizes)
    {
        series->Arg(size);
    }
    series->ArgName("N")
        ->Iterations(SortsPerRepetition)
        ->Repetitions(Repetitions)
        ->UseRealTime()
        ->Unit(benchmark::kMicrosecond);
}
} // namespace

auto main(int argc, char **argv) -> int
{
    try
    {
        benchmark::Initialize(&argc, argv);
        if (benchmark::ReportUnrecognizedArguments(argc, argv))
        {
            return EXIT_FAILURE;
        }
        const unsigned parallelWorkers = ReadParallelWorkers();
        const Seed seed = MakeSeed();
        bool failed = false;
        benchmark::AddCustomContext("seed", std::to_string(seed));
        benchmark::AddCustomContext("generator", "std::mt19937 + std::uniform_int_distribution");
        benchmark::AddCustomContext("verification", "std::ranges::sort + std::ranges::equal");
        benchmark::AddCustomContext("seq_workers", std::to_string(SequentialWorkers));
        benchmark::AddCustomContext("par_workers", std::to_string(parallelWorkers));
        RegisterSort("seq", false, parallelWorkers, seed, failed);
        RegisterSort("par", true, parallelWorkers, seed, failed);
        RunsAndMedianReporter reporter;
        benchmark::RunSpecifiedBenchmarks(&reporter);
        benchmark::Shutdown();
        return failed ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
