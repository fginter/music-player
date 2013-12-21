#!/bin/bash

INFILE="$HOME/otto-videot/krot_i_raketa-6pVYcU46vKw.flv"

function xtract {
    outName="rak/r$1"
    shift
    ffmpeg -i $INFILE -ac 1 -ar 20000 -acodec pcm_u16le $* -f u16le ${outName}.u16
    cat ${outName}.u16 | python packbits.py > ${outName}_20.da
}

#ffmpeg -i $1 -ac 1 -ar 11025 -acodec pcm_u16le -f u16le ${outName}_11.u16
#cat ${outName}_11.u16 | python packbits.py > ${outName}_11.da

xtract 00 -ss 00:00:00 -t 19
xtract 01 -ss 00:01:12 -t 28
xtract 02 -ss 00:01:40 -t 10
xtract 03 -ss 00:03:03 -t 7
xtract 04 -ss 00:03:36 -t 22
xtract 05 -ss 00:04:00 -t 7
xtract 06 -ss 00:04:28 -t 30
xtract 07 -ss 00:06:22 -t 8
xtract 08 -ss 00:06:35 -t 15
xtract 09 -ss 00:07:04 -t 21
xtract 10 -ss 00:07:28 -t 12
xtract 11 -ss 00:07:42 -t 24
xtract 12 -ss 00:08:04 -t 16
xtract 13 -ss 00:08:20 -t 17
