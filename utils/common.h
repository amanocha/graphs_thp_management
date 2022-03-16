#include <iostream>
#include <assert.h>
#include <fstream>
#include <chrono>
#include <string>
#include "graph.h"
#include <unistd.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/resource.h>

#define MAX_EDGES 4094967295
#define weight_max 1073741823

using namespace std;

int lock_memory(char* addr, size_t size) {
    cout << "Called lock_memory with " << (void*) addr << " and " << size << endl;
    unsigned long page_offset, page_size;
    page_size = sysconf(_SC_PAGE_SIZE);
    page_offset = (unsigned long) addr % page_size;

    addr -= page_offset;  /* Adjust addr to page boundary */
    size += page_offset;  /* Adjust size with page_offset */

    cout << "Locking " << size << "B of memory" << endl;
    return (mlock(addr, size));  /* Lock the memory */
}

int unlock_memory(char* addr, size_t size) {
    unsigned long page_offset, page_size;

    page_size = sysconf(_SC_PAGE_SIZE);
    page_offset = (unsigned long) addr % page_size;

    addr -= page_offset;  /* Adjust addr to page boundary */
    size += page_offset;  /* Adjust size with page_offset */

    return ( munlock(addr, size) );  /* Unlock the memory */
}

void pin_data(csr_graph G, unsigned long node) {
    unsigned long start, end, num_edges;
    int err;
    
    start = G.node_array[node];
    end = G.node_array[node+1];
    num_edges = end-start;
   
    err = lock_memory((char*) &start, num_edges * sizeof(unsigned long));
    if (err != 0) perror("Error with pinning edge data!");
}

void unpin_data(csr_graph G, unsigned long node) {
    unsigned long start, end, num_edges;
    int err;
    
    start = G.node_array[node];
    end = G.node_array[node+1];
    num_edges = end-start;
    
    err = unlock_memory((char*) &start, num_edges * sizeof(unsigned long));
    if (err != 0) perror("Error with unpinning edge data!");
}

int count_directory(const char* dir_name, const char* prefix) {
    DIR *d;
    struct dirent *dir;
    d = opendir(dir_name);
    int num = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            std::string name = dir->d_name;
            if (name.rfind(prefix, 0) == 0) num++;
        }
        closedir(d);
    }
    return num;
}

csr_graph parse_bin_files(string base, int run_kernel=0, int is_bfs=0) {
  csr_graph ret;
  ifstream nodes_edges_file(base + "num_nodes_edges.txt");
  unsigned long nodes, edges;
  int err;
  void *tmp = nullptr;
  auto start = chrono::system_clock::now();
  FILE *fp;
 
  nodes_edges_file >> nodes;
  nodes_edges_file >> edges;
  nodes_edges_file.close();
  cout << "found " << nodes << " " << edges << "\n";
  
  ret.nodes = nodes;
  ret.edges = edges;
  if (run_kernel == 200) {
    posix_memalign(&tmp, 1 << 21, ret.nodes * sizeof(unsigned long));
    int err;
    err = madvise(tmp, ret.nodes * sizeof(unsigned long), MADV_HUGEPAGE);
    if (err != 0) perror("Error!");
    else cout << "madvise successful!" << endl;

    /*
    err = lock_memory((char*) tmp, ret.nodes * sizeof(unsigned long));
    if (err != 0) perror("Error!");
    */
    
    ret.node_array = static_cast<unsigned long*>(tmp);
    
    posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
    ret.edge_array = static_cast<unsigned long*>(tmp);

    if (is_bfs == 0) {
      posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
      ret.edge_values = static_cast<weightT*>(tmp);
    }
  } else if (run_kernel == 300) {
    posix_memalign(&tmp, 1 << 21, (ret.nodes+1) * sizeof(unsigned long));
    ret.node_array = static_cast<unsigned long*>(tmp);
    
    posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
    int err;
    err = madvise(tmp, ret.edges * sizeof(unsigned long), MADV_HUGEPAGE);
    if (err != 0) perror("Error!");
    else cout << "madvise successful!" << endl;

    /*
    err = lock_memory((char*) tmp, ret.edges * sizeof(unsigned long));
    if (err != 0) perror("Error!");
    */
    
    ret.edge_array = static_cast<unsigned long*>(tmp);
    
    if (is_bfs == 0) {
      posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
      ret.edge_values = static_cast<weightT*>(tmp);
    }
  } else if (run_kernel == 400) {
    posix_memalign(&tmp, 1 << 21, (ret.nodes+1) * sizeof(unsigned long));
    ret.node_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.nodes + 1));
    
    posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
    ret.edge_array = static_cast<unsigned long*>(tmp);
    
    posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
    int err;
    err = madvise(tmp, ret.edges * sizeof(unsigned long), MADV_HUGEPAGE);
    if (err != 0) perror("Error!");
    else cout << "madvise successful!" << endl;

    /*
    err = lock_memory((char*) tmp, ret.edges * sizeof(unsigned long));
    if (err != 0) perror("Error!");
    */

    ret.edge_values = static_cast<weightT*>(tmp);
  } else {
    int err;

    posix_memalign(&tmp, 1 << 21, (ret.nodes+1) * sizeof(unsigned long));
    ret.node_array = static_cast<unsigned long*>(tmp);

    posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
    ret.edge_array = static_cast<unsigned long*>(tmp);
    
    if (is_bfs == 0) {
      posix_memalign(&tmp, 1 << 21, ret.edges * sizeof(unsigned long));
      ret.edge_values = static_cast<weightT*>(tmp);
    }
  }

  // ***** NODE ARRAY *****
  fp = fopen((base + "node_array.bin").c_str(), "rb");

  if (ret.edges > MAX_EDGES) {
    cout << "reading byte length of:    " << (ret.nodes + 1) * sizeof(unsigned long) << endl;
    for (unsigned long n = 0; n < ret.nodes + 1; n++) {
      fread(&ret.node_array[n], sizeof(unsigned long), 1, fp);
    }
  } else {
    cout << "reading byte length of:    " << (ret.nodes + 1) * sizeof(unsigned int) << endl;
    
    for (unsigned long n = 0; n < ret.nodes + 1; n++) {
      fread(&ret.node_array[n], sizeof(unsigned int), 1, fp);
    }
  }

  fclose(fp);
  ret.node_array[0] = 0;

  // ***** EDGE ARRAY *****
  fp = fopen((base + "edge_array.bin").c_str(), "rb");

  if (ret.edges > MAX_EDGES) {
    cout << "reading byte length of:    " << (ret.edges) * sizeof(unsigned long) << endl;
    
    for (unsigned long e = 0; e < ret.edges; e++) {
      fread(&ret.edge_array[e], sizeof(unsigned long), 1, fp);
    }
  } else {
    cout << "reading byte length of:    " << (ret.edges) * sizeof(unsigned int) << endl;
    
    for (unsigned long e = 0; e < ret.edges; e++) {
      fread(&ret.edge_array[e], sizeof(unsigned int), 1, fp);
    }
  }

  fclose(fp);

  // ***** VALUES ARRAY *****
  if (is_bfs == 0) {
    fp = fopen((base + "edge_values.bin").c_str(), "rb");
    cout << "reading byte length of:    " << (ret.edges) * sizeof(weightT) << endl;

    for (unsigned long e = 0; e < ret.edges; e++) {
      fread(&ret.edge_values[e], sizeof(weightT), 1, fp); 
    }

    fclose(fp);        
  }

  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "Reading graph elapsed time: " << elapsed_seconds.count() << "s\n";
  fflush(stdout);

  return ret;  
}

csc_graph parse_bin_files_csc(string base, int run_kernel=0) {
  csc_graph ret;
  ifstream nodes_edges_file(base + "csc_num_nodes_edges.txt");
  unsigned long nodes, edges;

  auto start = chrono::system_clock::now();
  
  nodes_edges_file >> nodes;
  nodes_edges_file >> edges;
  nodes_edges_file.close();
  cout << "found " << nodes << " " << edges << "\n";
  
  ret.nodes = nodes;
  ret.edges = edges;
  if (run_kernel == 2) {
    ret.node_array = (unsigned long *) mmap(NULL, sizeof(unsigned long) * (ret.nodes+1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (ret.node_array == MAP_FAILED) printf("Mapping nodes failed\n");
    ret.edge_array = (unsigned long *) mmap(NULL, sizeof(unsigned long) * (ret.edges), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (ret.edge_array == MAP_FAILED) printf("Mapping edges failed\n");
    ret.edge_values = (int *) mmap(NULL, sizeof(int) * (ret.edges), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (ret.edge_values == MAP_FAILED) printf("Mapping vals failed\n");
  } else if (run_kernel == 8) { // node array
    ret.node_array = (unsigned long *) mmap(NULL, sizeof(unsigned long) * (ret.nodes+1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (ret.node_array == MAP_FAILED) printf("Mapping nodes failed\n");
    ret.edge_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.edges));
    ret.edge_values = (weightT*) malloc(sizeof(weightT) * (ret.edges));
  } else if (run_kernel == 9) { // edge array
    ret.node_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.nodes + 1));
    ret.edge_array = (unsigned long *) mmap(NULL, sizeof(unsigned long) * (ret.edges), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (ret.edge_array == MAP_FAILED) printf("Mapping edges failed\n");
    ret.edge_values = (weightT*) malloc(sizeof(weightT) * (ret.edges));
  } else {
    ret.node_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.nodes + 1));
    ret.edge_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.edges));
    ret.edge_values = (weightT*) malloc(sizeof(weightT) * (ret.edges));
  }

  cout << "done allocating" << endl;

  ifstream node_array_file;
  node_array_file.open(base + "csc_node_array.bin", ios::in | ios::binary);
  
  if (!node_array_file.is_open()) {
    assert(0);
  }

  cout << "reading byte length of:    " << (ret.nodes + 1) * sizeof(unsigned long) << endl;
  node_array_file.read((char *)ret.node_array, (ret.nodes + 1) * sizeof(unsigned long));

  node_array_file.close();

  ifstream edge_array_file;
  edge_array_file.open(base + "csc_edge_array.bin", ios::in | ios::binary);
  
  if (!edge_array_file.is_open()) {
    assert(0);
  }
  
  cout << "reading byte length of:    " << (ret.edges) * sizeof(unsigned long) << endl;
  edge_array_file.read((char*)ret.edge_array, (ret.edges) * sizeof(unsigned long));
  
  edge_array_file.close();

  ifstream edge_values_file;
  edge_values_file.open(base + "csc_edge_values.bin", ios::in | ios::binary);
  if (!edge_values_file.is_open()) {
    assert(0);
  }
    
  edge_values_file.close();        

  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "Reading graph elapsed time: " << elapsed_seconds.count() << "s\n";

  return ret;
}
