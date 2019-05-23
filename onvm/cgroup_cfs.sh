#!/bin/bash 
# Example: ./cgroup_cfs.sh 0
# Example: ./cgroup_cfs.sh 1 [cgroup_name] [pid]
# Example: ./cgroup_cfs.sh 2 [cgroup_name] [share_percent]

#SCRIPT OPTIONS
#    0 = List tasks of the cgroup $2.
#    1 = add task $3 to the cgroup $2.
#    1 = set cpu.shares to $3 for cgroup $2.

#Assign Input args
cg_operation=$1
cg_name=$2
cg_task=$3
cg_base_dir="/sys/fs/cgroup/cpu"
cg_share=$3

#Assign globals
cg_exists=false

do_test_cfs_cgroup () {
        local __ret_val=$1
        local ret=false
        if [ -d $cg_base_dir/$cg_name ] ; then
                echo "cgroup $cg_base_dir/$cg_name Exits!"
                ret=true
                eval __ret_val="'$ret'"
                cg_exists=true
                return 1
        else
                echo  "cgroup $cg_base_dir/$cg_name Does Not Exit!"
                eval __ret_val="'$ret'"
                cg_exists=false
                return 0
        fi
}

do_list_cfs_group_shares() {

        CWD=$(pwd)
        cd $cg_base_dir
        for i in $(find . -maxdepth 1 -type d ); do
                cg_name=$(basename $i)
                #echo $(basename ${i})
                #cd $(basename $i)
                do_test_cfs_cgroup
                #echo "cg_exist=$cg_exists"

                if [ ! $cg_exists  -o  $cg_exists == "false" ]    
                #if [ "$cg_exists" -eq 0 ] || [ "$cg_exists" -eq 0 ]  
                then
                        echo " cgroupd $cg_name doest not exist!"
                        
                else
                        echo "\n cgroup $cg_name has cpu.share:"
                        cat   $cg_base_dir/$cg_name/cpu.shares
                fi
        done        
        cd $CWD
        return 1
}

do_list_cfs_tasks() {
        do_test_cfs_cgroup
        #echo "cg_exist=$cg_exists"
        if [ ! $cg_exists  -o  $cg_exists == "false" ]    
        #if [ "$cg_exists" -eq 0 ] || [ "$cg_exists" -eq 0 ]  
        then
                echo " cgroupd $cg_name doest not exist!"
                return 0
        else
                cat   $cg_base_dir/$cg_name/tasks
        fi        
        return 1
}

do_test_and_create_cfs_cgroup() {
        do_test_cfs_cgroup
        if [ ! $cg_exists  -o  $cg_exists == "false" ] ; then
                echo " Cgroupd $cg_name doest not exist creating a new one!"
                mkdir $cg_base_dir/$cg_name
                sync
        fi 
        return 1
}

do_add_task_to_cfs_group() {
        if ps -p $cg_task > /dev/null
        then
                #echo "$cg_task is running"
                echo $cg_task > $cg_base_dir/$cg_name/tasks
                do_list_cfs_tasks
        fi
}

do_set_cgroup_share_percent() {
        do_test_cfs_cgroup
        if [ ! $cg_exists  -o  $cg_exists == "false" ] ; then
                echo " Cgroupd $cg_name doest not exist!"
                do_test_and_create_cfs_cgroup
        fi
        cg_share_val="1024"
        case $cg_share in
        "0.25" | "25")
                cg_share_val=$(($cg_share_val / 4 ))
        ;;

        "0.5" | "50")
                cg_share_val=$(($cg_share_val / 2))
        ;;

        "0.75" | "75")
                cg_share_val=$(($cg_share_val * 3 /4 ))
        ;;

        "1" | "100" )
        ;;

        "2" | "200" )
                cg_share_val=$(($cg_share_val * 2))
        ;;

        "3" | "300" )
                cg_share_val=$(($cg_share_val * 3))
        ;;

        "4" | "400" )
                cg_share_val=$(($cg_share_val * 4))
        ;;

        "5" | "500" )
                cg_share_val=$(($cg_share_val * 5))
        ;;
        *)
            #if [ $cg_share -le 1 ] ; then
                #t=$(echo $cg_share_val * $cg_share | bc)
                #echo "t= $t"
                #cg_share_val=$((echo $cg_share_val * $cg_share | bc))
                #cg_share_val=$(($cg_share_val * $cg_share))
            #elif [ $cg_share -ge 100 ]; then
            if [ $cg_share -ge 100 ]; then
                cg_share_val=$(($cg_share_val * $cg_share /100))
            else
                cg_share_val=$(($cg_share_val * $cg_share /100))
                #cg_share_val=$(echo $cg_share_val * $cg_share | bc)
            fi
        esac

        echo "Setting the cg_share to : $cg_share: $cg_share_val"
        echo $cg_share_val > $cg_base_dir/$cg_name/cpu.shares
}

#Check Input args and process actions 
if [ -z $cg_operation ] ; then
        exit 0
elif [ $cg_operation -eq "-1" ] ; then
        do_list_cfs_group_shares
        exit 0
elif [ $cg_operation -eq "0" ] ; then
        do_list_cfs_tasks
        exit 0
elif [ $cg_operation -eq "1" ] ; then
        #do_test_and_create_cfs_cgroup
        do_add_task_to_cfs_group
        exit 0
elif [ $cg_operation -eq "2" ] ; then
        do_set_cgroup_share_percent
        exit 0
#elif [  $cg_operation -gt "2" -a   $cg_operation -le "5" ] ; then
#        do_set_predef_vals
#        exit 0
fi
