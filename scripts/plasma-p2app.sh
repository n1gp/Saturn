#!/bin/bash

# Script to start / stop p2app
# and send a notification to screen

APP=$(pidof p2app)
BINLOC=/home/pi/github/Saturn/sw_projects/P2_app

if [ -z "${APP}" ]; then
	$BINLOC/p2app -p &
	notify-send "p2app has been STARTED" -i  /home/pi/saturn.png -a P2APP
else
	killall -9 p2app
	notify-send "p2app has been STOPPED" -i  /home/pi/saturn.png -a P2APP
fi
