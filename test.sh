#!/bin/bash
set -e
ali=./build/ali
for file in $(find ./test -type f | grep -E '.al$'); do
  diff <($ali $file) $file.txt
  echo "OK '$file'"
done
