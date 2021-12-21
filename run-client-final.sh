#!/bin/sh
#
# This is the configuration of the final run.
# The seed value will be a non-zero value that will not be disclosed. 
#

SERVER=192.168.101.10
PORT=5003
SEED=5041
TOTAL=100
START=0
DIFFICULTY=10000
REP_PROB_PERCENT=20
DELAY_US=750000
PRIO_LAMBDA=1.5

/home/vagrant/os-challenge-common/client $SERVER $PORT $SEED $TOTAL $START $DIFFICULTY $REP_PROB_PERCENT $DELAY_US $PRIO_LAMBDA

