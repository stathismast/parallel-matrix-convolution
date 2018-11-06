# Parallel Matrix Convolution in C using MPI and openMP

- Course Project of Parallel  Systems Course.

## Compile

- From the version desired version directory, execute:
```
    mpicc gray.c -o gray -lm

    or

    mpicc color.c -o color -lm
```

## Execute

- To apply the filter on a gray .raw image
```
    mpiexec -n <number_of_processes> gray [-i inputFileName] [-o outputFileName]
    [-s rowsNumber colsNumber] [-f filterApplications]
```

- To apply the filter on a colored .raw image
```
    mpiexec -n <number_of_processes> color [-i inputFileName] [-o outputFileName]
    [-s rowsNumber colsNumber] [-f filterApplications]
```

## Examples

- Some examples of input files can be found in the ./waterfall/ directory
