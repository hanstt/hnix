#!/bin/sh

T_day=6500
T_night=3500
# When the night stops, e.g. 1930 = 7:30 pm.
t_1=700
# When the day starts.
t_2=900
# When the day stops.
t_3=1700
# When the night starts
t_4=1900

decimalify()
{
	echo "$1*60/100 + $1%100" | bc
}

t_1=`decimalify $t_1`
t_2=`decimalify $t_2`
t_3=`decimalify $t_3`
t_4=`decimalify $t_4`

while true
do
	t=`date +%H%M`
	t=`decimalify $t`
	if [ $t -lt $t_1 ]
	then
		T=$T_night
	elif [ $t -lt $t_2 ]
	then
		T=`echo "$T_night+($T_day-$T_night)*($t-$t_1)/($t_2-$t_1)" | bc`
	elif [ $t -lt $t_3 ]
	then
		T=$T_day
	elif [ $t -lt $t_4 ]
	then
		T=`echo "$T_day-($T_day-$T_night)*($t-$t_3)/($t_4-$t_3)" | bc`
	else
		T=$T_night
	fi
	sct $T
	sleep 60
done
