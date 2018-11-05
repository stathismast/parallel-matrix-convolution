# Application of filter to a large image using Convolution Matrix in Parallel Programming

- Course Project of Parallel  Systems Course.

- Implemented in C using MPI.

- Used techniques like Foster's Methodology, overlapping communications with
calculations,  and Cartesian Topology.

## Compile

- Being in the version folder you want, run:
```
    mpicc gray.c -o gray -lm

    or

    mpicc color.c -o color -lm
```

## Execute

- For gray image:
```
    mpiexec -n <number_of_processes> gray [-i inputFileName] [-o outputFileName]
    [-s rowsNumber colsNumber] [-f filterApplications]

    or

    mpiexec -f ../machines -n <number_of_processes> gray [-i inputFileName]
    [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications]
```

- For color image:
```
    mpiexec -n <number_of_processes> color [-i inputFileName] [-o outputFileName]
    [-s rowsNumber colsNumber] [-f filterApplications]

    or

    mpiexec -f ../machines -n <number_of_processes> color [-i inputFileName]
    [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications]
```

## Examples

- You can find examples of input files into waterfall/ folder.
