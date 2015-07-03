Histogram Merger
================

This program can be used to merge histograms from different ROOT files, even if the content doesn't match, classes are unknown to ROOT, or other cases where hadd will fail. In order to accomplish this you can control the program with arguments. A short version of all options will be listed with `-h` or `--help`.


Building
--------

The installation and proper configuration of the [ROOT framework](http://root.cern.ch/ "ROOT") is required. If it is installed, you can compile the source with the following command
```
g++ -std=c++11 -O3 merge_histograms.cpp -o meh `root-config --cflags --glibs` -lSpectrum -lHistPainter
```
where the word right after the `-o` flag is used as the executable's name. Change this to your needs.


Usage
-----

Following the example above, the name of the executable is meh (for MErging Histograms). You need to specify what files should be used as input (`-d` or `-i`), where the output should be saved (`-o`), and which histograms should be considered (`-p`). With `-h` or `--help` a short help message will be printed.

The input can be specified by either using `-d` or `--directory` followed by the path to a directory from where all .root-files will be read in or with `-i` or `--input-file` with a path to a file which contains a list of files which should be used. With `-o` or `--output` you specify where the ROOT file containing the merged histograms should be saved. The flag `-p` or `--plots` controls which histograms should be considered during the merging process. If you use the keywork `all` then all histograms which can be found in the first file will be read from the files and merged. You can also add a whitespace-separated list of the histogram names after the flag to merge only the listed histograms.


Non-ROOT classes
----------------

If you want to merge files which contain your own classes or other classes which don't belong to the standard ROOT classes, you might need to link against the corresponding library as well while compiling the executable. Otherwise ROOT could get stuck in a infinite loop while searching for a histogram. Let's say you have a file containing for example histograms produced with `a2display` which should be merged as well, make sure to link against this library, e. g. if you installed AcquRoot or something else which contains `a2display` in `/home/user/my_program` with the usual build directory, you need to append the following to the compile command in order to get everything working: `-L/home/user/my_program/build/lib -la2display`. Please make also sure to add the path `/home/user/my_program/build/lib` to your `LD_LIBRARY_PATH`.
