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

#include "pr.h"

using namespace std;
extern int errno;

int main(int argc, char** argv) {
  string graph_fname;
  csr_graph G;
  unsigned long in_index, out_index, *in_wl, *out_wl;
  float *x, *in_r, *ret;
  chrono::time_point<chrono::system_clock> start, end;

  // Parse arguments
  assert(argc >= 2);
  graph_fname = argv[1];

  // Initialize data and create irregular data
  G = parse_bin_files(graph_fname, 0, 1);
  init_kernel(G.nodes, &x, &in_r, &in_index, &out_index, &in_wl, &out_wl, &ret);
  init_kernel_vals(G, &in_r, &in_index, &in_wl);

  // Execute app
  printf("\n\nstarting kernel\n");
  start = chrono::system_clock::now();
  kernel(G, x, in_r, ret, in_wl, &in_index, out_wl, &out_index,0 , 1); // part of program to perf stat
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
