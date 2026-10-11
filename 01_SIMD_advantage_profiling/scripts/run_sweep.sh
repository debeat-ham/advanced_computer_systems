#!/usr/bin/env bash
set -euo pipefail

output="results/size_sweep.csv"
mkdir -p results

# Write the CSV header once.
echo "build,kernel,dtype,n,stride,offset,trial,time_ms,correct" > "$output"

for n in 1024 8192 65536 262144 1048576 4194304 16777216; do
    for kernel in saxpy mul dot; do
        for build in scalar default native; do
            echo "Running $build $kernel N=$n"

            ./bin/bench_"$build" "$kernel" f32 "$n" 1 0 10 \
                | tail -n +2 \
                | sed "s/^/$build,/" >> "$output"
        done
    done
done

echo "Finished! Results saved to $output"
