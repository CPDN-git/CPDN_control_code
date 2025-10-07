
# cpdn_zip/unzip library

Glenn Carver, CPDN, Oct 2025

Please see CMakeLists.txt file for more details and build instructions.

## Purpose

This code was developed to replace the old boinc_zip and boinc_unzip
functions supplied with the BOINC software. These old functions are
not memory safe.

This new code implements only the zip and unzip functionality of the 
original BOINC code as that's all the CPDN code requires.

## ZipLib

This new code makes use of the [ZipLib library](https://github.com/DreamyCecil/ZipLib).

Although by default CPDN code uses zip compression, the ZipLib library also
supports bzip2 which generally offers higher (but slower) compression for
geophysical data.


