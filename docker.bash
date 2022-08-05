#!/bin/bash

set -xe

name="rt"
image="$name-builder"

docker build --tag "$image" --build-arg "username=$name" .

workdir="/home/$name"

docker run \
  --rm \
  --tty \
  --interactive \
  --volume "$(pwd):$workdir" \
  --workdir "$workdir" \
  --cap-add SYS_PTRACE \
  "$image"
