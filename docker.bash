#!/bin/bash

set -xe

name="rt"
image="$name-builder"

docker build --tag "$image" "$(dirname $0)"

workdir="/home/$name"

docker run \
  --rm \
  --tty \
  --interactive \
  --volume "$(pwd):$workdir" \
  --workdir "$workdir" \
  --cap-add SYS_PTRACE \
  "$image"
