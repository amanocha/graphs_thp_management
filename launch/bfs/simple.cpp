#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <vector>

#include "bfs.h"

using namespace std;
extern int errno;

int main(int argc, char** argv) {
  string graph_fname;
  csr_graph G;
  unsigned long start_seed, in_index, out_index, *ret, *in_wl, *out_wl;
  chrono::time_point<chrono::system_clock> start, end;

  // Parse arguments
  assert(argc >= 2);
  graph_fname = argv[1];
  if (argc >= 3) start_seed = atoi(argv[2]);
  else start_seed = 0; 

  // Initialize data and create irregular data
  G = parse_bin_files(graph_fname, 0, 1);
  init_kernel(G.nodes, start_seed, &in_index, &out_index, &in_wl, &out_wl, &ret);

  // Execute app
  printf("\n\nstarting kernel\n");
  start = chrono::system_clock::now();
  kernel(G, ret, in_wl, &in_index, out_wl, &out_index, 0, 1);
  end = std::chrono::system_clock::now();
  printf("ending kernel\n");
  
  // Measure execution time      
  chrono::duration<double> elapsed_seconds = end-start;
  printf("\ntotal kernel computation time: %f\n", elapsed_seconds.count());

#if defined(OUTPUT_RET)
  ofstream outfile("out.txt");
  for (unsigned long i = 0; i < G.nodes; i++) {
    outfile << ret[i] << "\n";
  }
  outfile.close();
#endif

  // Clean up
  free(ret); 
  free(in_wl);
  free(out_wl);
  clean_csr_graph(G);

  return 0;
}
