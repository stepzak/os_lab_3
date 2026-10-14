# Test inputs

All commands below are expected to be run from the project root after building:

```bash
cmake -S src -B build
cmake --build build
```

Run a valid case and compare its standard output with the expected file:

```bash
./build/os_lab3 --threads 2 < tests/valid/identity.in \
    > /tmp/identity.actual
diff -u tests/valid/identity.out /tmp/identity.actual
```

The timing line is written to standard error and therefore does not interfere
with the comparison.

Valid cases:

- `identity`: an identity kernel applied twice; the matrix must not change.
- `zero_repetitions`: `K = 0`; the source must be copied without convolution.
- `single_cell_repeated`: a `1 x 1` matrix and kernel, applied three times.
- `zero_padding`: checks processing near all four borders.
- `reflected_kernel`: asymmetric kernel that distinguishes convolution from
  correlation and checks kernel reflection.

Every file under `invalid/` must finish with a non-zero exit status:

```bash
./build/os_lab3 --threads 2 < tests/invalid/even_window.in
echo $?
```

Invalid cases cover incomplete input, zero/even dimensions, negative and
fractional sizes, malformed matrix elements, non-finite values, and truncated
matrix/kernel data.

## Benchmark input

`benchmark/large_512x512.in` contains a deterministic `512 x 512` matrix, a
normalized `9 x 9` box-blur kernel, and `10` repetitions. This corresponds to
about 212 million multiply-accumulate operations. Redirect the large result to
`/dev/null` while measuring:

```bash
./build/os_lab3 --threads 1 --input tests/benchmark/large_512x512.in > /dev/null
./build/os_lab3 --threads 2 --input tests/benchmark/large_512x512.in > /dev/null
./build/os_lab3 --threads 4 --input tests/benchmark/large_512x512.in > /dev/null
```

For example, a `perf` run can be started without stdin redirection:

```bash
perf stat -- ./build/os_lab3 -t 4 -i tests/benchmark/large_512x512.in \
    --no-output
```

`--no-output` disables conversion of the resulting matrix to text, which is
useful for `perf` and FlameGraph profiling.

The input can be regenerated deterministically with:

```bash
awk -f tests/benchmark/generate_large.awk \
    > tests/benchmark/large_512x512.in
```

The repetition count can be overridden when generating a longer profiling
workload:

```bash
awk -v repeat_count=500 -f tests/benchmark/generate_large.awk \
    > /tmp/large_512x512_k500.in
```

## FlameGraph

Build an optimized executable with debug symbols and frame pointers:

```bash
cmake -S src -B build-profile \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O3 -g -DNDEBUG -fno-omit-frame-pointer"
cmake --build build-profile -j
```

Record stacks for a sufficiently long computation:

```bash
perf record -F 997 -g --call-graph fp -o /tmp/os_lab3_perf.data -- \
    ./build-profile/os_lab3 -t 4 \
    -i /tmp/large_512x512_k500.in --no-output
```

On a hybrid Intel processor, `perf` records P-core and E-core cycles as
separate events. Collapse both event streams and combine them into one graph:

```bash
perf script -i /tmp/os_lab3_perf.data \
    | stackcollapse-perf.pl --event-filter='cpu_core/cycles/Pu' \
    > /tmp/os_lab3_core.folded
perf script -i /tmp/os_lab3_perf.data \
    | stackcollapse-perf.pl --event-filter='cpu_atom/cycles/Pu' \
    > /tmp/os_lab3_atom.folded
cat /tmp/os_lab3_core.folded /tmp/os_lab3_atom.folded \
    | flamegraph.pl --title 'Parallel convolution: 4 threads' \
    > tests/benchmark/flamegraph.svg
```
