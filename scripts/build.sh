#!/bin/bash

meson build
ninja -C build
meson install -C build --tags=onvm