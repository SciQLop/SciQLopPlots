#!/usr/bin/sh

podman run --rm -it -v $(pwd):/SciQLopPlots:z -e'PATH=/opt/Qt/6.9.2/gcc_64/bin/:$PATH' localhost/sciqlopplots_build:latest /opt/python/cp313-cp313/bin/python -m build -n /SciQLopPlots -Cdist-args=--allow-dirty -w
