#!/bin/bash

for file in "$@"; do
    echo "#include \"../$file\""
done
