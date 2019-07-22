#!/bin/sh
# This script correctly configures duktape for use with dukev

cd duktape
python tools/configure.py --output-directory=src -DDUK_USE_SYMBOL_BUILTIN
