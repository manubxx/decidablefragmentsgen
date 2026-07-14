#!/bin/bash
EXEC="../build/DecidableFragmentsGen" 
BASE_DIR="./gf_datasets"
COUNT=30 

echo "GENERATING GF DATASETS (COUNT=$COUNT)"
rm -rf "$BASE_DIR"

mkdir -p "$BASE_DIR/depthscaling"
mkdir -p "$BASE_DIR/arityscaling"
mkdir -p "$BASE_DIR/budgetlimits"
mkdir -p "$BASE_DIR/largescaling"

MODES=("unsat" "sat" "free")
LARGE_MODES=("unsat" "free")


for mode in "${MODES[@]}"; do
    for depth in 3 5 7 10; do
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment guarded --mode $mode --output tptp --count 1 --depth $depth --preds "8/6" \
                > "$BASE_DIR/depthscaling/${mode}_d${depth}_${i}.p"
        done
    done
done


for mode in "${MODES[@]}"; do
    for preds in "3/2" "4/3" "5/4" "6/5"; do
        safe_preds=$(echo $preds | tr '/' '_')
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment guarded --mode $mode --output tptp --count 1 --depth 6 --preds "$preds" \
                > "$BASE_DIR/arityscaling/${mode}_p${safe_preds}_${i}.p"
        done
    done
done


for mode in "unsat" "free"; do
    for and_budget in 1 5 9 15; do
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment guarded --mode $mode --output tptp --count 1 --depth 6 --preds "8/6" --and $and_budget \
                > "$BASE_DIR/budgetlimits/${mode}_and${and_budget}_${i}.p"
        done
    done
done


for mode in "${LARGE_MODES[@]}"; do
    for depth in 12 15 18; do
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment guarded --mode $mode --output tptp --count 1 --depth $depth --preds "10/8" \
                > "$BASE_DIR/largescaling/${mode}_d${depth}_${i}.p"
        done
    done
done

echo "DONE. Saved in  $BASE_DIR."