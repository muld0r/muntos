#!/bin/bash

set -e

scons -j $(nproc)

trap 'if [ "$?" != "0" ]; then echo "test failed!"; fi' EXIT

set -x

build/list
build/mutex
build/newtask
build/once
build/pq
build/queue
build/rwlock
build/sem
build/simple
build/sleep
build/water/cond
build/water/sem
