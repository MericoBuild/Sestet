#!/bin/bash

LOG_POST=".log"
MIN_STAL="24"
MAX_STAL="4096"

if [ $# -ne 3 ]; then
  echo "Usage: $0 TraceDataDirectory NumPeaks SlopeThreshold"
  exit 1
fi

trace_dir=$1
num_peaks=$2
thr_slope=$3

num_files=0
ratio_sum=0

for kern_file in `ls $trace_dir/*-kern-*.log`
do
  file_pre=${kern_file%$LOG_POST}

  python proc-kern-data.py $kern_file > $file_pre
  ./test_opt_loc.out $file_pre $MIN_STAL $MAX_STAL $num_peaks $thr_slope > $file_pre.data

  tmp=(`cat $file_pre.loc`)
  loc_x=${tmp[1]}
  ratio=${tmp[2]}
  num_files=`expr $num_files + 1`
  ratio_sum=`echo $ratio_sum + $ratio | bc -l`
  gnuplot -e "IN_FILE='$file_pre.data'; OUT_FILE='$file_pre.eps'; LOC_X='$loc_x'" opt-ratio.plt
  rm $file_pre
done

ratio_avg=`echo $ratio_sum / $num_files | bc -l`
echo "Ratio avg = $ratio_avg"

