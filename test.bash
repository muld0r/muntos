#!/bin/bash

set -xe

scons -j $(nproc)

build/simple
build/sleep
build/sem
build/queue
build/mutex
