#!/bin/bash

if [ -z $1 ] || [ -z $2 ]
then
	echo "Requires two arguments"
	exit -1
fi

filepath=$1
string=$2

dirpath=$(dirname $filepath)

mkdir -p $dirpath

touch $filepath

if [ ! $? -eq 0 ]
then
	exit 1
fi

echo $string > $filepath
