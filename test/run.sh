#!/bin/bash 
rm -f results.out
for filename in ./compiled/*.cfg; do
	hth_console $filename; 
done
for filename in ./compiled/*.txt; do
	python3 main.py $filename -d compiled/ -s; 
done 
find ./compiled/ -regextype "egrep" \
       -iregex '.*/[0-9].*\.txt' \
       -exec 	python3 main.py {} >> results.out 2>>results.err \;


