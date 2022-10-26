#!/bin/bash

set -e

scons -j $(nproc)

trap 'if [ "$?" != "0" ]; then echo "test failed!"; fi' EXIT

set -x
build/simple
build/sleep
build/sem
build/pq
build/sbheap
build/queue
build/mutex
build/water_sem
build/water_cond
