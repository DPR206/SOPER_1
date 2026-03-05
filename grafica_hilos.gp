set terminal png
set output "grafica_nrondas.png"

set title "Tiempo de ejecucion de miner"
set xlabel "Numero de rondas"
set ylabel "Número óptimo de hilos"
set grid

plot "num_rondas.log" using 1:2 with linespoints title "hilos"