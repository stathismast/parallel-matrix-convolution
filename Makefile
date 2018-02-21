#HOW TO USE
#make: compiles all available code
#make gray: runs finalGrey_Null for the default parameters
#make color: runs finalColor_Null for the default parameters
#make reducegray: runs finalGrey_Null_Reduce for the default parameters
#make reducecolor: runs finalColor_Null_Reduce for the default parameters

#You can append something like 'procs=16' to the above commands to change
#each parameter.
	#'procs' defines the number of processes the program runs on
	#'filter' defines the number of times the filter will be applied
	#'input' defines the input file name
	#'output' define the output file name
	#'rows' defines the height of the input image
	#'cols' defines the width of the input image
#For example, if you want to apply the filter 55 times, on 25 processes
#a 480x640 grayscale image named 'lake.raw' and output the result into a
#file named 'blurryLake.raw' you have to use the following 'make' command:
#'make gray filter=55 procs=25 cols=480 rows=640 input=lake.raw output=blurryLake.raw'

#You don't have to append anything if you just want the default values. Similarly,
#you can append any number of parameters you wish. Thus, something like
#'make color filter=50' is just as valid as the example mentioned above.

procs = 4
filter = 100
input = default
output = default
cols = 1920
rows = 2520

all:
	mpicc -o finalGrey_Null finalGrey_Null.c -lm ; mpicc -o finalColor_Null finalColor_Null.c -lm ;
	mpicc -o finalGrey_Null_Reduce finalGrey_Null_Reduce.c -lm ; mpicc -o finalColor_Null_Reduce finalColor_Null_Reduce.c -lm


gray:
ifeq ($(input),default)
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@mpiexec -n $(procs) finalGrey_Null -f $(filter) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalGrey_Null -f $(filter) -o $(output) -s $(rows) $(cols)
endif
else
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@mpiexec -n $(procs) finalGrey_Null -f $(filter) -i $(input) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalGrey_Null -f $(filter) -i $(input) -o $(output) -s $(rows) $(cols)
endif
endif


color:
ifeq ($(input),default)
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@mpiexec -n $(procs) finalColor_Null -f $(filter) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalColor_Null -f $(filter) -o $(output) -s $(rows) $(cols)
endif
else
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@mpiexec -n $(procs) finalColor_Null -f $(filter) -i $(input) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalColor_Null -f $(filter) -i $(input) -o $(output) -s $(rows) $(cols)
endif
endif


reducegray:
ifeq ($(input),default)
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@mpiexec -n $(procs) finalGrey_Null_Reduce -f $(filter) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalGrey_Null_Reduce -f $(filter) -o $(output) -s $(rows) $(cols)
endif
else
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@mpiexec -n $(procs) finalGrey_Null_Reduce -f $(filter) -i $(input) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalGrey_Null_Reduce -f $(filter) -i $(input) -o $(output) -s $(rows) $(cols)
endif
endif


reducecolor:
ifeq ($(input),default)
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@mpiexec -n $(procs) finalColor_Null_Reduce -f $(filter) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalColor_Null_Reduce -f $(filter) -o $(output) -s $(rows) $(cols)
endif
else
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@mpiexec -n $(procs) finalColor_Null_Reduce -f $(filter) -i $(input) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@echo Output file is $(output).
	@mpiexec -n $(procs) finalColor_Null_Reduce -f $(filter) -i $(input) -o $(output) -s $(rows) $(cols)
endif
endif
