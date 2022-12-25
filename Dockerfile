FROM debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
      apt-get install -y \
      bear \
      clang \
      gdb \
      less \
      llvm \
      scons

RUN useradd rt
USER rt
