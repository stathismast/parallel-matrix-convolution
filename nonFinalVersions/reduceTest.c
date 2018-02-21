#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc, char *argv[]) {
	int comm_sz;               /* Number of processes    */
	int my_rank;               /* My process rank        */

	char localChanges,globalChanges;

	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	if( my_rank == 0 ){

		localChanges = 0 ;

	}
	else if( my_rank == 1 ){

		localChanges = 1 ;

	}
	else if( my_rank == 2 ){

		localChanges = 0 ;

	}
	else if( my_rank == 3 ){

		localChanges = 0 ;

	}
	else if( my_rank == 4 ){

		localChanges = 0 ;

	}

	MPI_Reduce(&localChanges,&globalChanges,1,MPI_CHAR,MPI_LOR,0,MPI_COMM_WORLD);

	if( my_rank == 0 ){
		if( !globalChanges ){
			printf("There are no changes.\n" );
		}
		else{
			printf("There are changes somewhere.\n" );
		}
		//printf("globalChanges is %d \n",globalChanges );
	}

	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
