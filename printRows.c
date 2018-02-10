#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int comm_sz;               /* Number of processes    */
	int my_rank;               /* My process rank        */

	int **array2D; /* 2D array */
	int i;
	MPI_Datatype row;
	int myRow[5];
	MPI_Request request;
	
	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* Creating derived Datatype */
	MPI_Type_contiguous(5,MPI_INT,&row);
	MPI_Type_commit(&row);

	if (my_rank == 0) {
		/* Create array */
		array2D = malloc( 5 * sizeof(int*) ); /* 5 rows */

		for( i=0; i<comm_sz; i++ ){ /* comm_sz must be 5 */
			array2D[i] = malloc( 5 * sizeof(int) ); /*5 columns */
			array2D[i][0] = i*10 + 1;
			array2D[i][1] = i*10 + 2;
			array2D[i][2] = i*10 + 3;
			array2D[i][3] = i*10 + 4;
			array2D[i][4] = i*10 + 5;
		}

		/* Send rows to other processes */
		for( i=0; i<comm_sz; i++ ){
			MPI_Isend(array2D[i],1,row,i,0,MPI_COMM_WORLD,&request);
		}
		MPI_Recv(myRow,1,row,0,0,MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	else {
		/* Recieve row */
		MPI_Recv(myRow,1,row,0,0,MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}


	/* Print row */
	printf("Row of process %d is :\n",my_rank );
	for( i=0; i<5; i++ ){
		printf("%d ",myRow[i] );
	}
	printf("\n\n");

	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
