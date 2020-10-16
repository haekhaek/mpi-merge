#include "mpi.h"
#include <iostream>

int main(int argc, char *argv[]){
  int rank, size;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) {
    std::cout << "I'm the boss" << "\n";
  } else {
    std::cout << "I'm slave " << rank << "\n";
  }

  MPI_Finalize();
  return 0;
}
