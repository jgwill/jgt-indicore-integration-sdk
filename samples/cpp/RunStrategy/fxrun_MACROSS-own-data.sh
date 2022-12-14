#!/bin/bash

THIS_DIR=`cd -P "$(dirname "$0")" && pwd`
cd $THIS_DIR/bin
./RunStrategy -i $THIS_DIR/bin/indicators/standard/ -s $THIS_DIR/bin/strategies/standard/ -p /a/data/EURUSD_t1__2022.csv  -n MACROSS
