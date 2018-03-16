#!/bin/bash

# This shell script converts all files of extension ".jp2" in the current 
# directory to their corresponding ".jpeg" files in the specificied
# "outdir".  Modify extensions as needed to convert between different formats.

base_command="ffmpeg -i"
outdir="converted"
mkdir -p $outdir
for filename in ./*.jp2; do
  outfile=$outdir/$(basename $filename .jp2).jpeg
  full_command="$base_command $filename $outfile -loglevel error -y"
  echo Converting $filename to $outfile...
  $full_command
done
