#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int comm_sz;               /* Number of processes    */
	int my_rank;               /* My process rank        */

	MPI_Datatype type,resizedtype;

	int i;

	int array2D[6][6]; /* original array */
	int myArray[3][3]; /* local array of each process */

	/* Arguments of create_subarray */
	int sizes[2] = {6,6}; /* size of original array */
	int subsizes[2] = {3,3}; /* size of subArrays */
	int starts[2] = {0,0}; /* first subArray starts from index [0][0] */

	/* Arguments of Scatterv */
	int counts[4] = {1,1,1,1}; /* How many pieces of data everyone has in units of block */
	int displs[4] = {0,1,6,7}; /* The starting point of everyone's data in the global array, in block extents ( 3-ints )  */

	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* Creating derived Datatype */
	MPI_Type_create_subarray(2,sizes,subsizes,starts,MPI_ORDER_C,MPI_INT,&type);
	MPI_Type_create_resized(type,0,3*sizeof(int),&resizedtype);
	MPI_Type_commit(&resizedtype);

	if (my_rank == 0) {
		/* Fill in array */

		for( i=0; i<6; i++ ){ /* comm_sz must be 4 */

			array2D[i][0] = i*10 + 1;
			array2D[i][1] = i*10 + 2;
			array2D[i][2] = i*10 + 3;
			array2D[i][3] = i*10 + 4;
			array2D[i][4] = i*10 + 5;
			array2D[i][5] = i*10 + 6;
		}

	}

	MPI_Scatterv(array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 myArray,3*3,MPI_INT, /* I'm recieving 3*3 MPI_INTs into myArray */
				 0,MPI_COMM_WORLD );



	/* Print subArrays */
	printf("SubArray of process %d is :\n",my_rank );
	for( i=0; i<3; i++ ){
		printf("%d %d %d\n",myArray[i][0],myArray[i][1],myArray[i][2] );
	}
	printf("\n\n");

	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
