FROM ubuntu:xenial

# This docker image is based of instructions from
# https://github.com/parallaxinc/propgcc-docs/blob/master/doc/Build.md

RUN \
  apt-get update && \
  apt-get install -y \
    build-essential \
    git \
    gperf \
    libncurses5-dev \
    wget

# Install an old version of texinfo
# http://forums.parallax.com/discussion/comment/1216485#Comment_1216485
RUN \
  wget http://ftp.gnu.org/gnu/texinfo/texinfo-4.13a.tar.gz && \
  tar -zxvf texinfo-4.13a.tar.gz && \
  cd texinfo-4.13 && \
  ./configure && \
  make && \
  make install && \
  cd .. && \
  rm -rf texinfo-4.13 texinfo-4.13a.tar.gz

# Install OpenSpin
RUN \
  wget http://david.zemon.name:8111/repository/download/OpenSpin_LinuxX8664/lastSuccessful/openspin.tar.gz?guest=1 -O openspin.tar.gz && \
  tar xzf openspin.tar.gz && \
  mv openspin /usr/bin && \
  rm openspin.tar.gz

# Create the destination directory
RUN \
  mkdir /opt/parallax

# Get propgcc source code
RUN \
  git clone https://github.com/cepr/propgcc.git

# Build
RUN \
  cd propgcc && \
  make clean-all && \
  PATH=${PATH}:/opt/parallax/bin make

