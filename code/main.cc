#include <mpi.h>
#include <limits.h>

#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <vector>
#include <algorithm>

using std::vector;
using std::cout;
using std::sort;

struct MortonOrder{
  int x, y, z_value;
};

struct ReadPosition{
  int start, stop;
};

bool compZValue(const MortonOrder &a, const MortonOrder &b) {
  return a.z_value < b.z_value;
}

int shiftBits(uint64_t x, uint64_t y) {
  int xx = 0;
  int z_order = 0;

  for (int i = 0; i < (int)(sizeof(xx) * CHAR_BIT); i++) {
    z_order |= (x & 1U << i) << i | (y & 1U << i) << (i + 1);
  }
  return z_order;
}

vector<MortonOrder> create_morton_order(const int number_of_cores) {
  vector<MortonOrder> result;

  for (int y = 0; y < number_of_cores; ++y) {
    for (int x = 0; x < number_of_cores; ++x) {
      auto z_value = shiftBits(x, y);
      result.push_back({x, y, z_value});
    }
  }

  return result;
}

vector<ReadPosition> calc_read_pos(const int number_of_cores,
                                    vector<MortonOrder> result) {
  int counter = 0;
  vector<ReadPosition> read_positions;

  for (uint64_t i=0 ; i < result.size() ; i++) {
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
  const int number_of_cores = 8;

  MPI_Init(&argc, &argv);

  MPI_Comm comm = MPI_COMM_WORLD;

  int nr, me;
  MPI_Comm_rank(comm, &me);
  MPI_Comm_size(comm, &nr);

  MPI_Win win_read;
  MPI_Win win_write;
  MPI_Info info = MPI_INFO_NULL;

  using value_t = int;
  MPI_Aint size = number_of_cores * sizeof(value_t);
  MPI_Aint disp = sizeof(value_t);

  int* baseptr_read;
  int* baseptr_write;

  // collective / blocking
  auto res = MPI_Win_allocate(size, disp, info, comm, &baseptr_read, &win_read);
  auto res2 = MPI_Win_allocate(size, disp, info, comm, &baseptr_write, &win_write);

  // std::iota(baseptr, baseptr + 2, me * nr);
  // cout << res << "\n";
  //std::fill(baseptr, baseptr + 2, me);
  for (i = 0; i < number_of_cores; ++i) {
    baseptr_read = i;
  }

  // open transaction (nonblocking)
  MPI_Win_lock_all(0, win);

  // Business logic
  // ---------------------------------------------------------------------------
  vector< vector<int> > readBuff;
  vector<MortonOrder> result = create_morton_order(number_of_cores);
  vector<ReadPosition> read_pos = calc_read_pos(number_of_cores, result);

  sort(result.begin(), result.end(), compZValue);
  // if (me == 0) {
  //   for (auto foo : result) {
  //     cout << foo.x << "\t" << foo.y << "\t" << foo.z_value << "\n";
  //   }

  //   for (auto foo : read_pos) {
  //     cout << "\t" << foo.start << "\t" << foo.stop << "\n";
  //   }
  // }
  // ---------------------------------------------------------------------------

  ReadPosition start_stop = read_pos[me];

  std::ostringstream os0;
  os0 << "rank: " << me << " baseptr[0] = " << baseptr_read[0] << "\n";
  cout << os0.str();
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
  baseptr_write[1] = val;

  std::ostringstream os1;
  os1 << "rank: " << me << " baseptr = [";

  std::copy(baseptr_read, baseptr_read + 2, std::ostream_iterator<int>(os1, " "));

  os1 << "]\n";
  cout << os1.str();

  // free allocated memory (nonblocking)
  MPI_Win_free(&win);

  MPI_Finalize();
}
