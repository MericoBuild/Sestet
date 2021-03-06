set terminal postscript eps enhanced font 24
set size 1,0.7
set output "opl-r.eps"
set style data histograms
set style fill pattern 1
set style histogram cluster gap 1
set ylabel 'Avg r ratio (%)'
set yrange [0:100]
set key left
plot 'app-ratio.data' using 2:xtic(1) axes x1y1 title "Ext4*", \
    '' using 3:xtic(1) axes x1y1 title "AdaFS", \
    '' using 4:xtic(1) axes x1y1 title "Offline"
