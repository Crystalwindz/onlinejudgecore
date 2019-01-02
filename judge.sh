#!/bin/bash

cp judge ../judge/judge

sudo docker run --rm -u judge -v $1:/judge oj-docker:latest bin/bash -c "cd /judge && ./judge $2 $3 $4 $5 $6 $7"