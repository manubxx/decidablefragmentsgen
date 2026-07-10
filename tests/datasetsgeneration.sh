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

echo "GENERATING DATASET: ff_datasets"


$EXEC --fragment fluted --mode unsat --output tptp --count 5 --depth 8 --preds "5/3" \
    > "$BASE_DIR/ffbasescalation/test_debug.p"


fixed_depth=4
for arity in 5 8 10 12 15; do
    $EXEC --fragment fluted --mode unsat --output tptp --count $COUNT --depth $fixed_depth --preds "4/$arity" > "$BASE_DIR/ffhigharity/higharity_a${arity}.p"
done


for depth in {4..10}; do
    $EXEC --fragment fluted --mode sat --output tptp --count $COUNT --depth $depth --preds "5/2" > "$BASE_DIR/ffsat/sat_d${depth}.p"
done


for arity in 4 6; do
    $EXEC --fragment fluted --mode unsat --output tptp --count $COUNT --depth 12 --preds "10/$arity" > "$BASE_DIR/ffunsat/unsat_a${arity}_d12.p"
done


$EXEC --fragment fluted --mode free --output tptp --count 5 --depth 15 --preds "8/3" > "$BASE_DIR/fsat/fffree.p"


$EXEC --fragment fluted --mode unsat --output tptp --count 5 --depth 12 \
    --preds "5/3" --exists "2" --forall "2" --or "2" --and "1" \
    > "$BASE_DIR/ffunsat/ffquantifiers.p"

ì
$EXEC --fragment fluted --mode unsat --output tptp --count 5 --depth 10 \
    --preds "4/2" --or "2" --and "3" \
    > "$BASE_DIR/ffunsat/ffboolean.p"

$EXEC --fragment fluted --mode unsat --output tptp --count 5 --depth 8 \
    --preds "3/3" --eq "2" \
    > "$BASE_DIR/ffunsat/ffequal.p"
echo "ff_dataset generated in $BASE_DIR"