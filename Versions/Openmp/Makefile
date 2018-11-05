#HOW TO USE
#make: compiles color and gray programs
#make mpip: compiles color and gray programs with mpip
#make rungray: runs gray for the default parameters
#make runcolor: runs color for the default parameters

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
	mpicc -openmp -o gray gray.c -lm ; mpicc -openmp -o color color.c -lm ;

mpip:
	mpicc -openmp -o gray gray.c -L/usr/local/mpip3/lib -lmpiP -lm -lbfd  ; mpicc -openmp -o color color.c -L/usr/local/mpip3/lib -lmpiP -lm -lbfd ;

rungray:
ifeq ($(input),default)
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@mpiexec -f ../openmpmachines -n $(procs) gray -f $(filter) -s $(rows) $(cols) -verbose
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Output file is $(output).
	@mpiexec -f ../openmpmachines -n $(procs) gray -f $(filter) -o $(output) -s $(rows) $(cols)
endif
else
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@mpiexec -f ../openmpmachines -n $(procs) gray -f $(filter) -i $(input) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@echo Output file is $(output).
	@mpiexec -f ../openmpmachines -n $(procs) gray -f $(filter) -i $(input) -o $(output) -s $(rows) $(cols)
endif
endif


runcolor:
ifeq ($(input),default)
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@mpiexec -f ../openmpmachines -n $(procs) color -f $(filter) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Output file is $(output).
	@mpiexec -f ../openmpmachines -n $(procs) color -f $(filter) -o $(output) -s $(rows) $(cols)
endif
else
ifeq ($(output), default)
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@mpiexec -f ../openmpmachines -n $(procs) color -f $(filter) -i $(input) -s $(rows) $(cols)
else
	@echo Running on $(procs) processes for $(cols)x$(rows) pixels.
	@echo Filter is applied $(filter) times.
	@echo Input file is $(input).
	@echo Output file is $(output).
	@mpiexec -f ../openmpmachines -n $(procs) color -f $(filter) -i $(input) -o $(output) -s $(rows) $(cols)
endif
endif
