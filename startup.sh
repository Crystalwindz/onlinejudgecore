#!/bin/bash

hostname="192.168.43.175:3306"
username="root"
passwd="root"
schema="oj"
judge="./judge.sh"
workdir="/home/crystalwind/Code/SoftwareTest/judge"

./polling ${hostname} ${username} ${passwd} ${schema} ${judge} ${workdir}
