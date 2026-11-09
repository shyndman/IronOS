FROM alpine:3.21

RUN apk add --no-cache \
      bash \
      findutils \
      g++-arm-none-eabi \
      gcc-arm-none-eabi \
      git \
      make \
      newlib-arm-none-eabi \
      py3-pip \
      python3 && \
    python3 -m pip install --root-user-action=ignore --break-system-packages --no-cache-dir bdflib && \
    git config --system --add safe.directory /build/source

WORKDIR /build/source/source
