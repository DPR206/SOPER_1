#!/bin/bash
LOGFILE="tiempos.log"
PROG="./miner"

echo "num_hilos tiempo_ms" > $LOGFILE
for i in $(seq 2 100)
do
    inicio=$(date +%s%N)
    $PROG 0 17 $i > /dev/null
    fin=$(date +%s%N)
    tiempo=$(echo "scale=6; ($fin - $inicio) / 1000000" | bc -l)

    echo "$i $tiempo" >> $LOGFILE
done