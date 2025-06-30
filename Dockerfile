# syntax = docker/dockerfile:1.3-labs
FROM debian:trixie as builder

ARG WLROOTSVERSION=0.19.0
ARG WLROOTSLIBVERSION=0.19
ARG SWAYFXVERSION=0.5.2
ARG SCENEFXVERSION=0.4.1
ARG SCENEFXLIBVERSION=0.4

ENV LANG C.UTF-8

RUN apt update && apt install -y --no-install-recommends \
               build-essential \
               cmake \
               cdbs \
               devscripts \
               equivs \
               meson \
               wayland-protocols \
               libwayland-dev \
               libwayland-bin \
               libwayland-client0 \
               libdrm-dev \
               libxkbcommon-dev \
               libudev-dev \
               hwdata \
               libseat-dev \
               libgles-dev \
               libavutil-dev \
               libavcodec-dev \
               libavformat-dev \
               libgbm-dev \
               xwayland \
               libxcb-composite0-dev \
               libxcb-icccm4-dev \
               libxcb-res0-dev \
               libxcb-errors-dev \
               libinput-dev \
               libxcb-present-dev \
               libxcb-render-util0-dev \
               libxcb-xinput-dev \
               glslang-tools \
               glslang-dev \
               libpcre2-dev \
               libjson-c-dev \
               libgdk-pixbuf-2.0-dev \
               libsystemd-dev \
               scdoc \
               bash-completion \
               libpango1.0-dev \
               libcairo2-dev \
               libxcb-ewmh-dev \
               libdisplay-info-dev \
               libliftoff-dev \
               liblcms2-dev \
               libvulkan-dev \
               wget \
               bash \
               git \
               ca-certificates \
               && rm -rf /var/lib/apt/lists/*

WORKDIR /build

RUN wget https://gitlab.freedesktop.org/wlroots/wlroots/-/archive/$WLROOTSVERSION/wlroots-$WLROOTSVERSION.tar.gz
RUN tar -xf wlroots-$WLROOTSVERSION.tar.gz
RUN rm wlroots-$WLROOTSVERSION.tar.gz

RUN wget https://github.com/WillPower3309/swayfx/archive/refs/tags/$SWAYFXVERSION.tar.gz
RUN tar -xf $SWAYFXVERSION.tar.gz
RUN rm $SWAYFXVERSION.tar.gz

RUN wget https://github.com/wlrfx/scenefx/archive/refs/tags/$SCENEFXVERSION.tar.gz
RUN tar -xf $SCENEFXVERSION.tar.gz
RUN rm $SCENEFXVERSION.tar.gz

COPY build.sh .
RUN chmod 755 build.sh

ENTRYPOINT ["/build/build.sh"]

