#!/bin/bash


EXEC="../build/DecidableFragmentsGen" 

BASE_DIR="./ff_datasets"

# problems for each instance
COUNT=5

mkdir -p "$BASE_DIR/ffbaseescalation"
mkdir -p "$BASE_DIR/ffhigharity"
mkdir -p "$BASE_DIR/ffedgecases"

echo " START GENERATION DATASET FLUTED (UNSAT)"

echo "GENERATING DATASET 1: Base Escalation"

for depth in {2..8}; do
    preds_count=5 
   
    arity=$(( (depth / 2) + 1 ))

    echo "Base: Depth=$depth, Arity=$arity"
    $EXEC \
        --fragment fluted \
        --mode unsat \
        --output tptp \
        --count $COUNT \
        --depth $depth \
        --preds "$preds_count/$arity" \
        > "$BASE_DIR/ffbaseescalation/ff_baseesc_d${depth}_a${arity}.p"
done

echo "GENERATING DATASET 2: High Arity"

fixed_depth=4
preds_count=4

for arity in 5 8 10 12 15; do
    echo "High Arity: Depth=$fixed_depth, Arity=$arity"
    $EXEC \
        --fragment fluted \
        --mode unsat \
        --output tptp \
        --count $COUNT \
        --depth $fixed_depth \
        --preds "$preds_count/$arity" \
        > "$BASE_DIR/ffhigharity/ff_higharity_a${arity}.p"
done

echo "GENERATING DATASET 3: Edge Cases"

echo "Edge Case: (No ORs, max ANDs)"
$EXEC \
    --fragment fluted \
    --mode unsat \
    --output tptp \
    --count $COUNT \
    --depth 12 \
    --preds "5/2" \
    --or "0:0" \
    --and "5:15" \
    --implies "2:5" \
    > "$BASE_DIR/ffedgecases/ff_deepnarrow.p" 

echo "Edge Case: (max ORs)"
$EXEC \
    --fragment fluted \
    --mode unsat \
    --output tptp \
    --count $COUNT \
    --depth 6 \
    --preds "5/3" \
    --or "15:25" \
    > "$BASE_DIR/ffedgecases/ff_wide.p"

echo "Edge Case: NNF"
$EXEC \
    --fragment fluted \
    --mode unsat \
    --output tptp \
    --count $COUNT \
    --depth 6 \
    --preds "4/4" \
    --transform nnf \
    > "$BASE_DIR/ffedgecases/ff_edge_nnf.p"


echo "cleaning file"
rm -rf ./generated_output

echo " DONE"