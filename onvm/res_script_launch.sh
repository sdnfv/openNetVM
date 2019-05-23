#usage  <prog_name> <$1=Mode [ Baseline, Local_Replica or Remote_Replica]> <$2=Run count>
#./launch_res_tests.sh baseline r5
# Results will be stored at: /home/skulk901/dev/res_results/<$2>/<$1>

now=`date +%d_%m_%Y_%H`
#echo "r$2$now"

if [ -z $1 ] 
then
  mode="unknown"
else
  mode=$1
fi

if [-z $2 ]
then
  run="res_unset"
else
  run="r$2"
fi

#outdir="../../res_results/r$1$now"
outdir="../../res_results/$run/$mode"
echo "logs@ $outdir/onvm_${now}.log"

#mkdir -p $outdir && touch $outdir/onvm_${now}.log && script $outdir/onvm_${now}.log
mkdir -p $outdir && touch $outdir/onvm_${now}.log && script -a $outdir/onvm_${now}.log
