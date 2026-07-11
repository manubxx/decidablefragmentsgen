#!/bin/bash
EXEC="../build/DecidableFragmentsGen" 
BASE_DIR="./ff_datasets"
COUNT=5 

rm -rf "$BASE_DIR"
mkdir -p "$BASE_DIR/ffbasescalation"
mkdir -p "$BASE_DIR/ffhigharity"
mkdir -p "$BASE_DIR/ffsat"
mkdir -p "$BASE_DIR/ffunsat"
mkdir -p "$BASE_DIR/fsat"
mkdir -p "$BASE_DIR/ffthreshold" 


for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode unsat --output tptp --verify --count 1 --depth 8 --preds "5/3" \
        > "$BASE_DIR/ffbasescalation/base_esc_${i}.p"
done

fixed_depth=4
for arity in 5 8 10 12 15; do
    for i in $(seq 1 $COUNT); do
        $EXEC --fragment fluted --mode unsat --output tptp --verify --count 1 --depth $fixed_depth --preds "4/$arity" \
            > "$BASE_DIR/ffhigharity/higharity_a${arity}_${i}.p"
    done
done

for depth in {4..10}; do
    for i in $(seq 1 $COUNT); do
        $EXEC --fragment fluted --mode sat --output tptp --verify --count 1 --depth $depth --preds "5/2" \
            > "$BASE_DIR/ffsat/sat_d${depth}_${i}.p"
    done
done

for arity in 4 6; do
    for i in $(seq 1 $COUNT); do
        $EXEC --fragment fluted --mode unsat --output tptp --verify --count 1 --depth 12 --preds "10/$arity" \
            > "$BASE_DIR/ffunsat/unsat_a${arity}_d12_${i}.p"
    done
done

for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode free --output tptp --count 1 --depth 15 --preds "8/3" \
        > "$BASE_DIR/fsat/fffree_${i}.p"
done

for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode unsat --output tptp --verify --count 1 --depth 12 \
        --preds "5/3" --exists "2" --forall "2" --or "2" --and "1" \
        > "$BASE_DIR/ffunsat/ffquantifiers_${i}.p"
done

for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode unsat --output tptp --verify --count 1 --depth 10 \
        --preds "4/2" --or "2" --and "3" \
        > "$BASE_DIR/ffunsat/ffboolean_${i}.p"
done

for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode unsat --output tptp --verify --count 1 --depth 8 \
        --preds "3/3" --eq "2" \
        > "$BASE_DIR/ffunsat/ffequal_${i}.p"
done

for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode sat --output tptp --verify --count 1 --depth 16 --preds "20/5" \
        > "$BASE_DIR/ffthreshold/vocabsaturation_${i}.p"
done

for i in $(seq 1 $COUNT); do
    $EXEC --fragment fluted --mode sat --output tptp --verify --count 1 --depth 10 --preds "20/5" --exists 1 --forall 1 \
        > "$BASE_DIR/ffthreshold/quantfallback_${i}.p"
done

echo "Generated in $BASE_DIR"