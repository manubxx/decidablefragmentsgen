#!/bin/bash
EXEC="../build/DecidableFragmentsGen" 
BASE_DIR="./ff_datasets"
COUNT=30 

echo "GENERATING, COUNT=$COUNT for each instance"
rm -rf "$BASE_DIR"


mkdir -p "$BASE_DIR/depthscaling"
mkdir -p "$BASE_DIR/arityscaling"
mkdir -p "$BASE_DIR/budgetlimits"

MODES=("unsat" "sat" "free")

for mode in "${MODES[@]}"; do
    for depth in 5 8 11 15; do
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment fluted --mode $mode --output tptp --count 1 --depth $depth --preds "5/3" \
                > "$BASE_DIR/depthscaling/${mode}_d${depth}_${i}.p"
        done
    done
done


for mode in "${MODES[@]}"; do
    for preds in "3/2" "5/4" "10/6" "20/8"; do
        safe_preds=$(echo $preds | tr '/' '_')
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment fluted --mode $mode --output tptp --count 1 --depth 10 --preds "$preds" \
                > "$BASE_DIR/arityscaling/${mode}_p${safe_preds}_${i}.p"
        done
    done
done

for mode in "unsat" "free"; do
    for and_budget in 1 5 9 15; do
        for i in $(seq 1 $COUNT); do
            $EXEC --fragment fluted --mode $mode --output tptp --count 1 --depth 8 --preds "5/3" --and $and_budget \
                > "$BASE_DIR/budgetlimits/${mode}_and${and_budget}_${i}.p"
        done
    done
done

echo "DONE, saved in $BASE_DIR"