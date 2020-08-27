#!/bin/bash

lynx -dump -hiddenlinks=listonly http://dataphys.org/wiki/Bibliography | grep pdf | cut -f 2- -d"." | grep -v infoscape | while
 read riga
do
 wget -U mozilla -N "$riga"
done

rdfind -deleteduplicates true .
detox *
