#!/bin/bash

set -xe

scons -j $(nproc)

build/simple
build/sleep
build/sem
build/queue
build/mutex
build/water_sem
build/water_cond
