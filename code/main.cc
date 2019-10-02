#include <mpi.h>
#include <limits.h>

#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <vector>

struct MortonOrder{
  int x, y, z_value;
};

struct ReadPositions{
  int start, stop;
};

int shiftBits (uint64_t x, uint64_t y) {
  int xx = 0;
  int z_order = 0;

  for (int i = 0; i < (int)(sizeof(xx) * CHAR_BIT); i++) {
    z_order |= (x & 1U << i) << i | (y & 1U << i) << (i + 1);
  }
  return z_order;
}

std::vector<MortonOrder> create_morton_order(const int number_of_cores) {
  std::vector<MortonOrder> result;

  for (int y = 0; y < number_of_cores; ++y) {
    for (int x = 0; x < number_of_cores; ++x) {
      auto z_value = shiftBits(x, y);
      result.push_back({x, y, z_value});
    }
  }

  return result;
}

std::vector<ReadPositions> calc_read_pos(const int number_of_cores,
                                         std::vector<MortonOrder> result) {
  int counter = 0;
  std::vector<ReadPositions> read_positions;

  for (unsigned long i=0 ; i < result.size() ; i++) {
    if ((i % number_of_cores) == 0) {
      int start =  number_of_cores * counter;
      int stop = (number_of_cores * (counter + 1)) - 1;

      read_positions.push_back({start, stop});
      counter++;
    }
  }

  return read_positions;
}

int main(int argc, char* argv[]) {
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
  std::cout << res << "\n";
  std::fill(baseptr, baseptr + 2, me);

  // open transaction (nonblocking)
  MPI_Win_lock_all(0, win);

  // Business logic
  //----------------------------------------------------------------------------
  const int number_of_cores = 8;
  std::vector< std::vector<int> > readBuff;
  std::vector<MortonOrder> result = create_morton_order(number_of_cores);
  std::vector<ReadPositions> read_pos = calc_read_pos(number_of_cores, result);

  if (me == 0) {
    for (auto foo : result) {
      std::cout << foo.x << "\t" << foo.y << "\t" << foo.z_value << "\n";
    }

    for (auto foo : read_pos) {
      std::cout << "\t" << foo.start << "\t" << foo.stop << "\n";
    }
  }
  //----------------------------------------------------------------------------


  std::ostringstream os0;
  os0 << "rank: " << me << " baseptr[0] = " << baseptr[0] << "\n";
  std::cout << os0.str();
  MPI_Barrier(comm);

  auto target = (me - 1 + nr) % nr;

//  for (int i=read_pos[me].start; i < read_pos[me].stop; i++) {
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
//  }

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
