#!/bin/sh
set -x
SERVER=127.0.0.1
PORT=5003
SEED=1234
TOTAL=20
START=1
DIFFICULTY=100
REP_PROB_PERCENT=0
DELAY_US=10
PRIO_LAMBDA=1.5

./client $SERVER $PORT $SEED $TOTAL $START $DIFFICULTY $REP_PROB_PERCENT $DELAY_US $PRIO_LAMBDA
