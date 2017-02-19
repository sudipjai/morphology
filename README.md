# morphology
Implements vision morphological functions

It implements tiling feature to parallely execute block of image using
multithreading approach. The number of threads created are equal to number
of tiles, so each tile is processed in a seperate thread

The code is tested on X86 and ARM architectures, the performance signifcantly
improves to 50% when using multithreaded approach on images greater than 1080x1080.
However for smaller resolution,  default execution (single thread) is better

Download
--------
git clone https://github.com/sudipjai/morphology.git

Build
-----
make

Clean
-----
make clean

Usage
-----
./morphology pentagram_256x256.raw -d <num_dilations> -e <num_erosions>
Default value of num_dilation and num_erosion is 1, to disable set to 0

./morphology pentagram_256x256.raw -d 0 -e <num_erosions>
Note: If count > 0 erosion precedes over dilation

Tile
----
./morphology pentagram_256x256.raw -d <num_dilations> -e <num_erosions> -t <tilesize>
default tile size 64x64 i.e PAGE_SIZE(4096)

Each tile is executed in a seperate thread. Max Threads = 1024


Input/Output
------------
Input and output files are 'gray8' 255 BPP binary(headless) images with below naming format.

<shortname>_<w>_<h>.ext
Program reads the width and height from the filename

Output files are created with "out_" prefix to the input file name


Perf Results
-------------
CPU INFO: Intel(R) Core(TM) i7-5600U CPU @ 2.60GHz
CPUS: 4

Resolution 	TileSize 	Threads		Filter/Times 	Latency(ms)
1024x1024	1024x1024	1		Dilation/1	38.88
1024x1024	64x64		256		Dilation/1	24.78 (~50% improvement)


Limitation: This approach suits multicore CPU, same may not be
applicable for execution on GPGPU, unless using high level framework
such as Vulkan.
