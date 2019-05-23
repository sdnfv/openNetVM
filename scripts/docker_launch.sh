container_name=$1
if [ -z $1 ] 
then
  container_name="dummy_container"
fi
sudo ./docker.sh -h /mnt/huge -o /home/skulk901/dev/my_fork/tx_openNetVM-dev-sameer -n $container_name -D /dev/uio0,/dev/uio1
