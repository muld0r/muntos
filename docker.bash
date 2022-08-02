#!/bin/bash

set -xe

name="rt"
image="$name-builder"

docker build --tag "$image" .

workdir="/home/$name/$name"

docker run \
  --rm \
  --tty \
  --interactive \
  --volume "$(pwd):$workdir" \
  --workdir "$workdir" \
  --cap-add SYS_PTRACE \
  "$image"
