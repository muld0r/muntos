#!/bin/bash

set -xe

scons -j 8
build/simple
build/sleep
build/sem
build/queue
