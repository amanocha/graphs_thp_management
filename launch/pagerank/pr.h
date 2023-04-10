#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include "../../utils/common.h"
#include "../../utils/split_huge_page.h"

float alpha = 0.85;
float epsilon = 0.01;

void init_kernel(unsigned long num_nodes, float **x, float **in_r, unsigned long *in_index, unsigned long *out_index, unsigned long **in_wl, unsigned long **out_wl, float **ret) {
  //*ret = (float *) malloc(sizeof(float) * num_nodes);
  *x = (float *) malloc(sizeof(float) * num_nodes);
  *in_r = (float *) malloc(sizeof(float) * num_nodes);
  *in_wl = (unsigned long *) malloc(sizeof(unsigned long) * num_nodes);
  *out_wl = (unsigned long *) malloc(sizeof(unsigned long) * num_nodes);

  for (unsigned long v = 0; v < num_nodes; v++) {
    (*x)[v] = 1 - alpha;
    (*in_r)[v] = 0;
    (*ret)[v] = 0;
  }
  
  *in_index = 0;
  *out_index = 0;
}
 
void init_kernel_vals(csr_graph G, float **in_r, unsigned long *in_index, unsigned long **in_wl) { 
  for (unsigned long v = 0; v < G.nodes; v++) {
    unsigned long G_v = G.node_array[v+1]-G.node_array[v];
    for (unsigned long i = G.node_array[v]; i < G.node_array[v+1]; i++) {
      unsigned long w = G.edge_array[i];
      (*in_r)[w] = (*in_r)[w] + 1.0/G_v;
    }

    unsigned long index = *in_index;
    *in_index = index + 1;
    (*in_wl)[index] = v;
  }

  for (unsigned long v = 0; v < G.nodes; v++) {
    (*in_r)[v] = (1-alpha)*alpha*((*in_r)[v]);
  }
}

void kernel(csr_graph G, float *x, float *in_r, float *ret, unsigned long *in_wl, unsigned long *in_index, unsigned long *out_wl, unsigned long *out_index, int tid, int num_threads) {
  unsigned long v, G_v, w; 
  float new_r, r_old;

  int hop = 1;
  while (*in_index > 0) {
    printf("-- epoch %d %lu\n", hop, *in_index);
    for (unsigned long i = tid; i < *in_index; i+=num_threads) {
      v = in_wl[i];
      x[v] = x[v] + in_r[v];
      G_v = G.node_array[v+1]-G.node_array[v];
      new_r = in_r[v]*alpha/G_v;
      for (unsigned long e = G.node_array[v]; e < G.node_array[v+1]; e++) {
        w = G.edge_array[e];
        r_old = ret[w];
        ret[w] = ret[w] + new_r;
        if (ret[w] >= epsilon && r_old < epsilon) {
          unsigned long index = *out_index;
          *out_index = *out_index + 1;
          out_wl[index] = w;
        }
      }
      in_r[v] = 0;
    }

    unsigned long *tmp_wl = out_wl;
    out_wl = in_wl;
    in_wl = tmp_wl;
    hop++;
    *in_index = *out_index;
    *out_index = 0;

    float *tmp_r = ret;
    ret = in_r;
    in_r = tmp_r;
  }
}
