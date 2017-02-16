# morphology
Implements vision morphological functions

Download
========
git clone https://github.com/sudipjai/morphology.git

Build
=====
# make

Usage
====
# ./morphology pentagram_256x256.raw -d <num_dilations> -e <num_erosions>
default value of num_dilation and num_erosion is 1

Clean
====
make clean

Input and Output
================
Both input and output files needs to be "gray8" raw format (headless)

Input Filename format
=====================
<shortname>_<w>_<h>.ext
Program reads the width and height from the filename

Output Files
============
Output files are created with "out_" prefix to the input file name

Order
=====
It count > 0 erosion precedes over dilation
