#!/bin/bash

DATASETS_DIR="./ff_datasets"
RESULTS_FILE="./benchmark_results.csv"

VAMPIRE_EXEC="vampire" 

TIMEOUT=300 


echo "Dataset,FileName,SZS_Status,Time_Elapsed(s)" > "$RESULTS_FILE"

echo " START BENCHMARKING FF-VAMPIRE"
echo " Timeout: $TIMEOUT seconds"



for folder in "$DATASETS_DIR"/*; do
    if [ -d "$folder" ]; then
        dataset_name=$(basename "$folder")
        echo "[*] Analizzo Dataset: $dataset_name"

      
        for file in "$folder"/*.p; do
          
            [ -e "$file" ] || continue
            
            filename=$(basename "$file")
            echo -n " $filename ... "

            output=$($VAMPIRE_EXEC -t $TIMEOUT "$file" 2>&1)
            exit_code=$?


            szs_status=$(echo "$output" | grep -oP '(?<=SZS status )\w+' | head -n 1)
            
            
            time_elapsed=$(echo "$output" | grep -oP '(?<=Time elapsed: )\d+\.\d+' | head -n 1)

           
            if [ -z "$szs_status" ]; then
                if [ $exit_code -ne 0 ]; then
                    szs_status="Timeout_or_Error"
                    time_elapsed=$TIMEOUT
                else
                    szs_status="Unknown"
                    time_elapsed="N/A"
                fi
            fi

           
            if [ -z "$time_elapsed" ]; then
                time_elapsed="0.000"
            fi

            echo "[$szs_status] in ${time_elapsed}s"

            echo "$dataset_name,$filename,$szs_status,$time_elapsed" >> "$RESULTS_FILE"

        done
    fi
done


echo " DONE"
