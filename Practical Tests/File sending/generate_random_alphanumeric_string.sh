#!/bin/bash

dd bs=1 count=1000 if=/dev/urandom | base64 | tr +/ 0A > 3.txt
