# CW1: последовательный и параллельный quicksort на ParlayLib

## Сборка

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNATIVE=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

## Benchmark

```bash
build/quicksort_benchmark
```

Один процесс запускает обе версии и выводит **одну стандартную таблицу
Google Benchmark**. Именованный массив `InputSizes` задаёт только `10^4`,
`10^6` и `10^8` элементов. `Repetitions` задаёт пять повторов для каждого размера
и каждой версии; `SortsPerRepetition` — ровно одну сортировку свежего массива
в каждом повторе. seq использует один поток, число потоков par задаётся через
переменную окружения `PARLAY_NUM_THREADS` (по умолчанию 4).
Счётчики повторов, измерение времени, статистика и вывод принадлежат Google Benchmark.

В консоли для каждого сочетания версии и размера отображаются **пять отдельных
замеров и медиана**.

```bash
PARLAY_NUM_THREADS=4 taskset -c 0,2,4,6 build/quicksort_benchmark \
  --benchmark_enable_random_interleaving=true
```

taskset -c нужно для cpu affinity

## Результаты

Intel Core i7-1165G7,
4 физических ядра / 8 логических CPU (HyperThreading), WSL2 Linux x86-64,
GCC `16.0.1 20260315 (experimental)`, C++23,
Release с `-std=c++23 -O3 -DNDEBUG -march=native`.
Процесс закреплён за CPU `0,2,4,6`; seq использует один поток, par — четыре.

Конфигурация: общий median-of-three, `InsertionLimit = 1 << 5` (32),

| Элементов | Медиана seq, мкс | Медиана par, мкс | Ускорение seq/par |
| ---: | ---: | ---: | ---: |
| 10 000 | 428,738 | 492,311 | 0,87× |
| 1 000 000 | 68 623,752 | 21 344,352 | 3,22× |
| 100 000 000 | 9 354 720,584 | 3 116 049,250 | **3,00×** |

На `10^8` элементов по медианам: **seq — 9,354721 с, par — 3,116049 с, ускорение — 3,002×**.

Повторить данные этого прогона:

```bash
QUICKSORT_BENCHMARK_SEED=3021257236 PARLAY_NUM_THREADS=4 \
  taskset -c 0,2,4,6 build/quicksort_benchmark \
  --benchmark_enable_random_interleaving=true
```

Полный вывод последнего запуска Google Benchmark:

```text
kirill@DESKTOP-7FCFTQ6:~/par1$ PARLAY_NUM_THREADS=4   taskset -c 0,2,4,6 build/quicksort_benchmark   --benchmark_enable_random_interleaving=true
2026-11-22T21:39:31+03:00
Running build/quicksort_benchmark
Run on (8 X 1689.6 MHz CPU s)
CPU Caches:
  L1 Data 48 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 1280 KiB (x4)
  L3 Unified 12288 KiB (x1)
Load Average: 0.46, 0.60, 0.36
generator: std::mt19937 + std::uniform_int_distribution
par_workers: 4
seed: 3021257236
seq_workers: 1
verification: std::ranges::sort + std::ranges::equal
***WARNING*** ASLR is enabled, the results may have unreproducible noise in them.
------------------------------------------------------------------------------------------------------------------
Benchmark                                                        Time             CPU   Iterations UserCounters...
------------------------------------------------------------------------------------------------------------------
seq/N:1000000/iterations:1/repeats:5/real_time               63114 us        63114 us            1 items_per_second=15.8443M/s verified=1 workers=1
seq/N:1000000/iterations:1/repeats:5/real_time               64629 us        64630 us            1 items_per_second=15.4728M/s verified=1 workers=1
seq/N:1000000/iterations:1/repeats:5/real_time               68944 us        68941 us            1 items_per_second=14.5045M/s verified=1 workers=1
seq/N:1000000/iterations:1/repeats:5/real_time               85838 us        85836 us            1 items_per_second=11.6499M/s verified=1 workers=1
seq/N:1000000/iterations:1/repeats:5/real_time               70488 us        70489 us            1 items_per_second=14.1869M/s verified=1 workers=1
seq/N:1000000/iterations:1/repeats:5/real_time_median        68944 us        68941 us            5 items_per_second=14.5045M/s verified=1 workers=1
par/N:10000/iterations:1/repeats:5/real_time                   584 us          443 us            1 items_per_second=17.136M/s verified=1 workers=4
par/N:10000/iterations:1/repeats:5/real_time                   466 us          402 us            1 items_per_second=21.4672M/s verified=1 workers=4
par/N:10000/iterations:1/repeats:5/real_time                   383 us          265 us            1 items_per_second=26.0759M/s verified=1 workers=4
par/N:10000/iterations:1/repeats:5/real_time                   480 us          480 us            1 items_per_second=20.8199M/s verified=1 workers=4
par/N:10000/iterations:1/repeats:5/real_time                   531 us          407 us            1 items_per_second=18.8242M/s verified=1 workers=4
par/N:10000/iterations:1/repeats:5/real_time_median            480 us          407 us            5 items_per_second=20.8199M/s verified=1 workers=4
par/N:1000000/iterations:1/repeats:5/real_time               20498 us        18818 us            1 items_per_second=48.7858M/s verified=1 workers=4
par/N:1000000/iterations:1/repeats:5/real_time               18989 us        17380 us            1 items_per_second=52.6617M/s verified=1 workers=4
par/N:1000000/iterations:1/repeats:5/real_time               21196 us        18841 us            1 items_per_second=47.1783M/s verified=1 workers=4
par/N:1000000/iterations:1/repeats:5/real_time               22548 us        21275 us            1 items_per_second=44.3492M/s verified=1 workers=4
par/N:1000000/iterations:1/repeats:5/real_time               19528 us        18129 us            1 items_per_second=51.2076M/s verified=1 workers=4
par/N:1000000/iterations:1/repeats:5/real_time_median        20498 us        18818 us            5 items_per_second=48.7858M/s verified=1 workers=4
seq/N:10000/iterations:1/repeats:5/real_time                   487 us          486 us            1 items_per_second=20.5356M/s verified=1 workers=1
seq/N:10000/iterations:1/repeats:5/real_time                   451 us          448 us            1 items_per_second=22.1783M/s verified=1 workers=1
seq/N:10000/iterations:1/repeats:5/real_time                   479 us          478 us            1 items_per_second=20.8557M/s verified=1 workers=1
seq/N:10000/iterations:1/repeats:5/real_time                   567 us          566 us            1 items_per_second=17.6517M/s verified=1 workers=1
seq/N:10000/iterations:1/repeats:5/real_time                   505 us          504 us            1 items_per_second=19.8114M/s verified=1 workers=1
seq/N:10000/iterations:1/repeats:5/real_time_median            487 us          486 us            5 items_per_second=20.5356M/s verified=1 workers=1
par/N:100000000/iterations:1/repeats:5/real_time           3220703 us      3211932 us            1 items_per_second=31.0491M/s verified=1 workers=4
par/N:100000000/iterations:1/repeats:5/real_time           2933307 us      2903763 us            1 items_per_second=34.0912M/s verified=1 workers=4
par/N:100000000/iterations:1/repeats:5/real_time           3684985 us      3649865 us            1 items_per_second=27.1372M/s verified=1 workers=4
par/N:100000000/iterations:1/repeats:5/real_time           2887702 us      2882254 us            1 items_per_second=34.6296M/s verified=1 workers=4
par/N:100000000/iterations:1/repeats:5/real_time           2810890 us      2771828 us            1 items_per_second=35.5759M/s verified=1 workers=4
par/N:100000000/iterations:1/repeats:5/real_time_median    2933307 us      2903763 us            5 items_per_second=34.0912M/s verified=1 workers=4
seq/N:100000000/iterations:1/repeats:5/real_time           8984077 us      8983693 us            1 items_per_second=11.1308M/s verified=1 workers=1
seq/N:100000000/iterations:1/repeats:5/real_time           9513615 us      9512669 us            1 items_per_second=10.5113M/s verified=1 workers=1
seq/N:100000000/iterations:1/repeats:5/real_time           9445700 us      9444927 us            1 items_per_second=10.5868M/s verified=1 workers=1
seq/N:100000000/iterations:1/repeats:5/real_time           9266479 us      9265029 us            1 items_per_second=10.7916M/s verified=1 workers=1
seq/N:100000000/iterations:1/repeats:5/real_time           9336850 us      9335402 us            1 items_per_second=10.7103M/s verified=1 workers=1
seq/N:100000000/iterations:1/repeats:5/real_time_median    9336850 us      9335402 us            5 items_per_second=10.7103M/s verified=1 workers=1
```