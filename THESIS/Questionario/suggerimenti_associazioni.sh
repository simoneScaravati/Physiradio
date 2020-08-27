#!/bin/bash

CSV=dati_questionario.csv

csvtool namedcol "SUGGERIMENTI_ASSOCIAZIONI" $CSV|head -n 2 |tail -n 1|tr ";" "\n"|cut -f1 -d:|tr -s "\n " ":"
echo

csvtool namedcol "SUGGERIMENTI_ASSOCIAZIONI" $CSV|tail -n +2| while
 read riga
do
 echo $riga|tr -d '"'|tr -s " "|tr ";" "\n"|cut -f2 -d:|tr "\n" ":"
 echo
done
