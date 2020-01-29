#!/bin/bash
# Copyright (c) 2019 The SIN Core developers
# Auth: xtdevcoin
#
# modified by cyberd3vil
#
# this script control the status of node
# 1. node is active
# 2. infinitynode is ENABLED
# 3. if infinitynode is ENABLED compare blockheight with explorer and resync if frozen
# 4. infinitynode is not ENABLED
# 5. node is stopped by supplier - maintenance
# 6. node is frozen - dead lock
#
# Add in crontab when YOUR NODE HAS STATUS ENABLED:
# */5 * * * * /full_path_to/infinitynode_surveyor.sh
#
#
# TODO: 1. upload status of node to server for survey
#       2. chech status of node from explorer
#

sin_deamon_name="sind"
sin_deamon="/home/$(whoami)/sind"
sin_cli="/home/$(whoami)/sin-cli"

DATE_WITH_TIME=`date "+%Y%m%d-%H:%M:%S"`

# get current blockheight from SIN explorer and infinity node
exp_blockheight=$(curl -s http://explorer.sinovate.io/api/getblockcount)
mn_blockheight=$($sin_cli getblockcount)

function start_node() {
	echo "$DATE_WITH_TIME : delete caches files debug.log db.log fee_estimates.dat governance.dat mempool.dat mncache.dat mnpayments.dat netfulfilled.dat" >> ~/.sin/sin_control.log 
	cd ~/.sin && rm debug.log db.log fee_estimates.dat governance.dat mempool.dat mncache.dat mnpayments.dat netfulfilled.dat
	sleep 5
	echo "$DATE_WITH_TIME : Start sin deamon $sin_deamon" >> ~/.sin/sin_control.log
	echo "$DATE_WITH_TIME : SIN_START" >> ~/.sin/sin_control.log
	$sin_deamon &
}

function stop_start_node() {
	echo "$DATE_WITH_TIME : delete caches files debug.log db.log fee_estimates.dat governance.dat mempool.dat mncache.dat mnpayments.dat netfulfilled.dat" >> ~/.sin/sin_control.log 
	cd ~/.sin && rm debug.log db.log fee_estimates.dat governance.dat mempool.dat mncache.dat mnpayments.dat netfulfilled.dat
	echo "$DATE_WITH_TIME : kill process by name $sin_deamon_name" >> ~/.sin/sin_control.log
	echo "$DATE_WITH_TIME : SIN_STOP" >> ~/.sin/sin_control.log
	pgrep -f $sin_deamon_name | awk '{print "kill -9 " $1}' | sh >> ~/.sin/sin_control.log
	sleep 15
	echo "$DATE_WITH_TIME : Restart sin deamon $sin_deamon" >> ~/.sin/sin_control.log
	echo "$DATE_WITH_TIME : SIN_START" >> ~/.sin/sin_control.log
	$sin_deamon &
}

function resync_node() {
        echo "$DATE_WITH_TIME : kill process by name $sin_deamon_name" >> ~/.sin/sin_control.log
        echo "$DATE_WITH_TIME : SIN_STOP" >> ~/.sin/sin_control.log
        pgrep -f $sin_deamon_name | awk '{print "kill -9 " $1}' | sh >> ~/.sin/sin_control.log
        sleep 15
        echo "$DATE_WITH_TIME : Resyncing" >> ~/.sin/sin_control.log
        echo "$DATE_WITH_TIME : SIN_START" >> ~/.sin/sin_control.log
        $sin_deamon -reindex &
}

timeout --preserve-status 10 $sin_cli getblockcount
CHECK_SIN=$?
echo "$DATE_WITH_TIME : check status of sind: $CHECK_SIN" >> ~/.sin/sin_control.log
echo "$DATE_WITH_TIME : Explorer blockheight: $exp_blockheight" >> ~/.sin/sin_control.log
echo "$DATE_WITH_TIME : Infinity Node blockheight: $mn_blockheight" >> ~/.sin/sin_control.log

#node is active
if [ "$CHECK_SIN" -eq "0" ]; then
	echo "$DATE_WITH_TIME : sin deamon is active" >> ~/.sin/sin_control.log
	SINSTATUS=`$sin_cli masternode status | grep "successfully" | wc -l`

	#infinitynode is ENABLED
	if [ "$SINSTATUS" -eq "1" ]; then
		echo "$DATE_WITH_TIME : infinitynode is started." >> ~/.sin/sin_control.log
		
		# ping explorer webserver before comparing blockheight
		if ping -c 1 explorer.sinovate.io &> /dev/null ;then

			#resync infinitynode if blockheight is not equal to SIN explorer
			if [ "$mn_blockheight" -ge  "$exp_blockheight" ] || [ "$(($exp_blockheight - $mn_blockheight))" -eq "1" ];then
			    echo "$DATE_WITH_TIME : Blockheight is equal, no resync needed." >> ~/.sin/sin_control.log
			else
			    echo "$DATE_WITH_TIME : Blockheight not synced! Resyncing!" >> ~/.sin/sin_control.log
			    resync_node
			fi
			
		fi

	else
		echo "$DATE_WITH_TIME : node is synchronising...please wait!" >> ~/.sin/sin_control.log
	fi
fi

#node is stopped by supplier - maintenance
if [ "$CHECK_SIN" -eq "1" ]; then
	#find sind
	SIND=`ps -e | grep $sin_deamon_name | wc -l`
	if [ "$SIND" -eq "0" ]; then
		start_node
	else
		stop_start_node
	fi
fi

#command not found
if [ "$CHECK_SIN" -eq "127" ]; then
	echo "$DATE_WITH_TIME : Command not found. Please change the path of sin_deamon and sin_cli." >> ~/.sin/sin_control.log
fi

#node is frozen
if [ "$CHECK_SIN" -eq "143" ]; then
	echo "$DATE_WITH_TIME : sin deamon will be restarted...." >> ~/.sin/sin_control.log
	stop_start_node
fi

echo "$DATE_WITH_TIME : ------------------" >> ~/.sin/sin_control.log
