#!/usr/bin/python

import sys
import os

def read_read(infile):
	read = infile.readline()

	if not read:
		return None, None, None

	id1 = (read.split('/', 1)[0])[1:]
	strand = (read.split('/', 1)[1])[0]

	if strand != '2' and strand != '1':
		print("Wrong strands\n");
		return None, None, None

	delim = (read.split('/', 1)[0])[0]

	line = infile.readline()
	while line and line.find(delim) != 0:
		read += line
		line = infile.readline()

	if not line:
		return None, None, None

	infile.seek(infile.tell() - len(line))
	
	return id1, strand, read
	


if len(sys.argv) < 2:
	print("Usage: " + sys.argv[0] + " <source>")	
	sys.exit()

inFileName = sys.argv[1]
inFile = open(inFileName, "r")

fName, ext = os.path.splitext(inFileName)
pFile1 = open(fName + "_left" + ext, "w") 
pFile2 = open(fName + "_right" + ext, "w")
sFile1 = open(fName + "_single_left" + ext, "w") 
sFile2 = open(fName + "_single_right" + ext, "w")


id1, st1, read1 = read_read(inFile)
id2, st2, read2 = read_read(inFile)

while id1 is not None and id2 is not None:
	if (id1 != id2):
		if st1 == '1':
			sFile1.write(read1)
		else:
			sFile2.write(read1)

		id1, st1, read1 = id2, st2, read2
		id2, st2, read2 = read_read(inFile)
		
	else:
		pFile1.write(read1)
		pFile2.write(read2)

		id1, st1, read1 = read_read(inFile)
		id2, st2, read2 = read_read(inFile)


inFile.close()
pFile1.close()
pFile2.close()
sFile1.close()
sFile2.close()

