#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <vector>

#include <mpi.h>

int main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);

  MPI_Comm comm = MPI_COMM_WORLD;

  int nr, me;
  MPI_Comm_rank(comm, &me);
  MPI_Comm_size(comm, &nr);

  MPI_Win  win;
  MPI_Info info = MPI_INFO_NULL;

  using value_t = int;
  MPI_Aint size = 2 * sizeof(value_t);
  MPI_Aint disp = sizeof(value_t);

  int* baseptr;

  // collective / blocking
  auto res = MPI_Win_allocate(size, disp, info, comm, &baseptr, &win);
  // std::iota(baseptr, baseptr + 2, me * nr);
  std::fill(baseptr, baseptr + 2, me);

  // open transaction (nonblocking)
  MPI_Win_lock_all(0, win);

  // Business logic
  std::ostringstream os0;
  os0 << "rank: " << me << " baseptr[0] = " << baseptr[0] << "\n";
  std::cout << os0.str();
  MPI_Barrier(comm);

  auto target = (me - 1 + nr) % nr;

  MPI_Put(
      // origin address
      &me,
      // count
      1,
      // type
      MPI_INT,
      // target rank
      target,
      // target displacement (offset)
      0,
      // count
      1,
      // type
      MPI_INT,
      // win
      win);

  int val;
  MPI_Get(&val, 1, MPI_INT, target, 1, 1, MPI_INT, win);

  // close transaction (commit, nonblocking)
  MPI_Win_unlock_all(win);

  MPI_Barrier(comm);
  baseptr[1] = val;

  std::ostringstream os1;
  os1 << "rank: " << me << " baseptr = [";

  std::copy(baseptr, baseptr + 2, std::ostream_iterator<int>(os1, " "));

  os1 << "]\n";
  std::cout << os1.str();

  // free allocated memory (nonblocking)
  MPI_Win_free(&win);

  MPI_Finalize();
}
