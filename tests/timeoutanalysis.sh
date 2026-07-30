#!/bin/bash

BENCHMARK_EXEC="./DecidableFragmentsGen" 
TARGET_DIR="../tests/gf_datasets/timeout_cand"
REPORT_DIR="benchmarkreport"

if [ ! -d "$TARGET_DIR" ] || [ -z "$(ls -A $TARGET_DIR)" ]; then
    echo "No formula found"
    exit 0
fi


TIMEOUTS=(30 60 120)

for t in "${TIMEOUTS[@]}"; do

    echo " Timeout: ${t}s"

    $BENCHMARK_EXEC --run-benchmarks "$TARGET_DIR" --vampire-timeout $t
    
    if [ -f "$REPORT_DIR/report_benchmark_casc.csv" ]; then
        mv "$REPORT_DIR/report_benchmark_casc.csv" "$REPORT_DIR/report_timeout_${t}s.csv"
        echo "DONE. Saved in: $REPORT_DIR/report_timeout_${t}s.csv"
    else
        echo "Error"
    fi
done

