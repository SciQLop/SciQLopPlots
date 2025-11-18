#!/usr/bin/sh

podman build -t  sciqlopplots_build --build-arg QT_VERSION=6.10.0  --build-arg PYTHON_VERSION=cp313-cp313 .
