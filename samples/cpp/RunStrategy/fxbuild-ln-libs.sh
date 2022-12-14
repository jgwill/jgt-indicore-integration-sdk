#!/bin/sh

THIS_DIR=`cd -P "$(dirname "$0")" && pwd`

[ -e CMakeCache.txt ] && rm CMakeCache.txt
[ -e CMakeFiles ] && rm -r CMakeFiles

cmake . && make

#mkdir -p $THIS_DIR/bin/indicators
ln -sf /opt/StrategyRunner/strategies $THIS_DIR/bin/strategies
ln -sf /opt/StrategyRunner/indicators $THIS_DIR/bin/indicators
#mkdir -p $THIS_DIR/bin/strategies/standard/
#cp -r $THIS_DIR/../../../indicators/standard/* $THIS_DIR/bin/indicators/standard/
#cp -r $THIS_DIR/../../../strategies/standard/* $THIS_DIR/bin/strategies/standard/
