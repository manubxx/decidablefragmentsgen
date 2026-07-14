#!/bin/bash
EXEC="../build/DecidableFragmentsGen" 
BASE_DIR="./ff_large"
COUNT=3 

echo "GENERATING large FF DATASETS, COUNT=$COUNT for each instance"
rm -rf "$BASE_DIR"

mkdir -p "$BASE_DIR/largescaling"

MODES=("unsat" "free") 

for mode in "${MODES[@]}"; do
    for depth in 12 15 18; do
        for i in $(seq 1 $COUNT); do
            # Ricorda il flag --fragment fluted
            $EXEC --fragment fluted --mode $mode --output tptp --count 1 --depth $depth --preds "20/12" \
                > "$BASE_DIR/largescaling/${mode}_d${depth}_${i}.p"
        done
    done
done

echo "DONE, Large datasets saved in $BASE_DIR"