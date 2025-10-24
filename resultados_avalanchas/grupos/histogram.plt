set terminal qt persist

set title "Distribución normalizada de tamaños de avalancha"
set xlabel "Tamaño de avalancha s"
set ylabel "Densidad de probabilidad"
set logscale y
set style fill solid 0.7 border -1
set boxwidth 50
binw = 200
bin(x) = binw * floor(x/binw) + binw/2.0

stats "datos_avalanchas_chi0.7_outlet4.19.csv" using 1 nooutput; N1 = STATS_records - 1
stats "datos_avalanchas_chi0.8_outlet4.19.csv" using 1 nooutput; N2 = STATS_records - 1
stats "datos_avalanchas_chi0.9_outlet4.19.csv" using 1 nooutput; N3 = STATS_records - 1
stats "datos_avalanchas_chi1.0_outlet4.19.csv" using 1 nooutput; N4 = STATS_records - 1

plot \
 "datos_avalanchas_chi0.7_outlet4.19.csv" using (bin($1)-75):(1.0/(N1*binw)) smooth freq with boxes lc rgb "blue" title "χ=0.7", \
 "datos_avalanchas_chi0.8_outlet4.19.csv" using (bin($1)-25):(1.0/(N2*binw)) smooth freq with boxes lc rgb "green" title "χ=0.8", \
 "datos_avalanchas_chi0.9_outlet4.19.csv" using (bin($1)+25):(1.0/(N3*binw)) smooth freq with boxes lc rgb "orange" title "χ=0.9", \
 "datos_avalanchas_chi1.0_outlet4.19.csv" using (bin($1)+75):(1.0/(N4*binw)) smooth freq with boxes lc rgb "red" title "χ=1.0"

