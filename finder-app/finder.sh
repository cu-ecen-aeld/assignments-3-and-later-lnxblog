#!/bin/bash

if [ -z $1 ] || [ -z $2 ]
then
	echo "Requires two arguments"
	exit -1
fi

desdir=$1
string=$2


if [ ! -d $desdir ]
then
	echo "$desdir Directory does not exist"
	exit -1
fi

file_count=$(grep -lr $string $desdir | wc -l)
#word_count=$(grep -or $string $desdir | wc -l)
line_count=$(grep -r $string $desdir | wc -l)
echo "The number of files are $file_count and the number of matching lines are $line_count"
