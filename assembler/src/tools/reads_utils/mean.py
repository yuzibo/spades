#!/usr/bin/python -O

import sys
import math

inFile = open(sys.argv[1])

ttl = 0.0
cnt = 0.0

while (1):
	line = inFile.readline()

	if not line:
		break

	ttl += int(line.split(' ')[0]) * int(line.split(' ')[1])
	cnt += int(line.split(' ')[1])

mean = ttl/cnt

inFile.close()
inFile = open(sys.argv[1])

dev = 0.0
med = 0
mcnt = 0
while (1):
       	line = inFile.readline()

       	if not line:
               	break

	ins = int(line.split(' ')[0])
       	dev += (ins - mean) * (ins - mean) * int(line.split(' ')[1])

	mcnt += int(line.split(' ')[1])
	if mcnt < cnt / 2:
		med = ins 
	

print("Mean: " + str(mean) + "\nDeviation: " + str(math.sqrt(dev / cnt)) + "\nMedian: " + str(med))

inFile.close()
