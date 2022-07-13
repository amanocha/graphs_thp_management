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

#include "sssp.h"

#define MAX_BUFFER_SIZE 1000
#define APP_TIME 1000000
#define SLEEP_TIME 1000000
#define KB_SIZE 1024
#define THP_SIZE_KB 2048
#define PERCENT_INTERVAL 0.05

using namespace std;
extern int errno;

const char* done_filename = "done.txt";
string smaps_filename, stat_filename;
int pid, pidfd;
unsigned long num_nodes, num_edges, num_thp_nodes, total_num_thps;
unsigned long **node_addr, **edge_addr, **ret_addr;

struct iovec iov;
float threshold;

// Set up irregularly accessed data
void create_irreg_data(int run_kernel, unsigned long** ret) {
  void *tmp = nullptr;
  posix_memalign(&tmp, 1 << 21, num_nodes * sizeof(unsigned long));
  *ret = static_cast<unsigned long*>(tmp);
  
  if (run_kernel >= 1 && run_kernel <= 20) {
      num_thp_nodes = (unsigned long) num_nodes*run_kernel*PERCENT_INTERVAL;
      cout << "Applying THPs to " << num_thp_nodes << "/" << num_nodes << endl;
      
      int err = madvise(*ret, num_nodes * sizeof(unsigned long), MADV_HUGEPAGE);
      if (err != 0) perror("Error!");
      else cout << "MADV_HUGEPAGE ret successful!" << endl;
    } else if (run_kernel == 100) {
      num_thp_nodes = num_nodes;
      
      int err = madvise(*ret, num_nodes * sizeof(unsigned long), MADV_HUGEPAGE);
      if (err != 0) perror("Error!");
      else cout << "MADV_HUGEPAGE ret successful!" << endl;

      /*
      int err = lock_memory((char*) ret, num_nodes * sizeof(unsigned long));
      if (err != 0) perror("Error!");
      */
    } else {
      num_thp_nodes = num_nodes;
    }
    
    fflush(stdout);
}

// Set up perf thread
void launch_perf(int cpid, const char* perf_cmd) {
  struct stat done_buffer;
  char buf[MAX_BUFFER_SIZE];
  sprintf(buf, perf_cmd, pid);

  printf("Perf process spawned! cpid %d waiting to run %s\n", cpid, buf);
  while (stat (done_filename, &done_buffer) != 0) {}
  printf("cpid %d running %s\n", cpid, buf);

  int err = execl("/bin/sh", "sh", "-c", buf, NULL);
  if (err == -1) printf("Error with perf!\n");
  fflush(stdout);
}

void demote_pages(unsigned long *curr_num_thps) { 
  ssize_t out;
  unsigned long pages_to_promote, pages_to_demote;
  unsigned long demotions = 0;
  
  pages_to_promote = (unsigned long) (threshold*total_num_thps);
  pages_to_demote = (unsigned long) total_num_thps-pages_to_promote;
 
  fprintf(stderr, "demoting: potential pages to demote = %lu\n\tstart = %lu, end = %lu\n\tbefore THPs = %lu, ", pages_to_demote, pages_to_promote, pages_to_promote+pages_to_demote, *curr_num_thps);
  iov.iov_len = pmd_pagesize;
 
  // Demote cold pages based on threshold
  iov.iov_base = (char*) *ret_addr+pages_to_promote*pmd_pagesize;
  iov.iov_len = pages_to_demote*pmd_pagesize;
  out = syscall(SYS_process_madvise, pidfd, &iov, 1, MADV_DEMOTE, 0);
  if (out < 0) perror("Error!");
  
  *curr_num_thps = check_huge((unsigned long*) *ret_addr, smaps_filename.c_str())/THP_SIZE_KB;
  fprintf(stderr, "after THPs = %lu, demotions = %lu\n", *curr_num_thps, demotions);
}

void launch_thp_tracking(int cpid, int run_kernel, const char* thp_filename, const char* pf_filename, unsigned long max_demotion_scans) {
  struct stat done_buffer, buffer;
  string cmd;
  unsigned long anon_size, thp_size, curr_num_thps;
  unsigned long promotion_scans = 0, promotions = 0, demotion_scans = 0, offset;
  unsigned int idx = 0;
  long thp_diff, num_thps_promote;
  vector<tuple<double, unsigned long, unsigned long>> thps_over_time, page_faults_over_time; 
  unsigned long *ret = *ret_addr;
  int soft_pf1, hard_pf1, soft_pf2, hard_pf2, soft_pf, hard_pf;

  setup_pagemaps(pid);
  total_num_thps = (num_nodes*sizeof(unsigned long)+pmd_pagesize-1)/pmd_pagesize;
  
  /*
  cmd = "taskset -cp 0 " + to_string(pid);
  printf("Binding process to core: %s, ", cmd.c_str());
  fflush(stdout);
  system(cmd.c_str());
  */

  printf("THP tracking process spawned! cpid %d waiting to run\n", cpid);
  while (stat (done_filename, &done_buffer) != 0) {}
  printf("cpid %d running\n", cpid);
  fflush(stdout);

  get_pfs(stat_filename.c_str(), &soft_pf1, &hard_pf1);
  
  auto glob_start = chrono::system_clock::now();
  while(0 == kill(pid, 0)) {
    // Track # THPs over time
    auto end = std::chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = end-glob_start;
    anon_size = check_huge(*ret_addr, smaps_filename.c_str(), "Anonymous:");
    thp_size = check_huge(*ret_addr, smaps_filename.c_str());
    if (anon_size == -1 || thp_size == -1) break;
    curr_num_thps = thp_size/THP_SIZE_KB;
    thps_over_time.push_back(make_tuple(elapsed_seconds.count(), anon_size, thp_size));

    get_pfs(stat_filename.c_str(), &soft_pf2, &hard_pf2);
    soft_pf = soft_pf2 - soft_pf1;
    hard_pf = hard_pf2 - hard_pf1;
    page_faults_over_time.push_back(make_tuple(elapsed_seconds.count(), soft_pf, hard_pf));
 
    // Demotion logic
    if (demotion_scans >= max_demotion_scans) {
      usleep(SLEEP_TIME);
      continue;
    }

    if (run_kernel > 0) {
      demote_pages(&curr_num_thps);
    }

    usleep(SLEEP_TIME);
  }
         
  ofstream thp_file(thp_filename);
  for (tuple<double, unsigned long, unsigned long> data : thps_over_time) {
    thp_file << to_string(get<0>(data)) + " sec: " + to_string(get<1>(data)) + " anon, " + to_string(get<2>(data)) + " THPs\n";
  }
  thp_file.close(); 

  ofstream pf_file(pf_filename);
  for (tuple<double, unsigned long, unsigned long> data : page_faults_over_time) {
    pf_file << to_string(get<0>(data)) + " sec: " + to_string(get<1>(data)) + " soft pf, " + to_string(get<2>(data)) + " hard pf\n";
  }
  pf_file.close(); 

  return; 
}

void launch_app(string graph_fname, int run_kernel, unsigned long start_seed) {
  csr_graph G;
  unsigned long *ret, in_index, out_index, *in_wl, *out_wl;
  double user_time1, kernel_time1, user_time2, kernel_time2;
  chrono::time_point<chrono::system_clock> start, end;
  int rusage_out = 0;
  struct rusage usage1, usage2;

  // Initialize data and create irregular data
  G = parse_bin_files(graph_fname, run_kernel, 0);

  create_irreg_data(run_kernel, &ret);
  init_kernel(num_nodes, start_seed, &in_index, &out_index, &in_wl, &out_wl, &ret);

  *node_addr = G.node_array;
  *edge_addr = G.edge_array;
  *ret_addr = ret;

  setup_pagemaps(pid); 
  total_num_thps = (num_nodes*sizeof(unsigned long)+pmd_pagesize-1)/pmd_pagesize;

  // Signal to other processes that app execution will start
  ofstream output(done_filename);
  output.close();
  usleep(APP_TIME);

  // First rusage
  rusage_out = getrusage(RUSAGE_SELF, &usage1);
  if (rusage_out != 0) perror("Error with getrusage!");
 
  // Execute app
  printf("\n\nstarting kernel\n");
  start = chrono::system_clock::now();
  get_times(stat_filename.c_str(), &user_time1, &kernel_time1);
  kernel(G, ret, in_wl, &in_index, out_wl, &out_index, 0 , 1); // part of program to perf stat
  get_times(stat_filename.c_str(), &user_time2, &kernel_time2);
  end = std::chrono::system_clock::now();
  printf("ending kernel\n");
  
  // Second rusage
  rusage_out = getrusage(RUSAGE_SELF, &usage2);
  if (rusage_out != 0) perror("Error with getrusage!");

  // Measure execution time      
  chrono::duration<double> elapsed_seconds = end-start;
  printf("\ntotal kernel computation time: %f\n", elapsed_seconds.count());
  printf("user time: %f\nkernel time: %f\n" , (user_time2-user_time1), (kernel_time2-kernel_time1));
  printf("footprint: start = %luKB, end = %luKB, diff = %luKB\npage reclaims: %lu\npage faults: %lu\nswaps: %lu\n", usage1.ru_maxrss, usage2.ru_maxrss, (usage2.ru_maxrss-usage1.ru_maxrss), (usage2.ru_minflt-usage1.ru_minflt), (usage2.ru_majflt-usage1.ru_majflt), (usage2.ru_nswap-usage1.ru_nswap));
  fflush(stdout);

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
}

int main(int argc, char** argv) {
  string graph_fname;
  int run_kernel = 0;
  const char *perf_cmd, *thp_filename = "thp.txt", *pf_filename = "pf.txt", *size_filename = "num_nodes_edges.txt";
  unsigned long start_seed = 0, max_demotion_scans = ULLONG_MAX;
  struct bitmask *parent_mask = NULL;

  assert(argc >= 2);
  graph_fname = argv[1];
  if (argc >= 3) run_kernel = atoi(argv[2]);
  if (argc >= 4) start_seed = atoi(argv[3]);
  if (argc >= 5) perf_cmd = argv[4];
  else perf_cmd = "perf stat -p %d -B -v -e dTLB-load-misses,dTLB-loads -o stat.log";
  if (argc >= 6) thp_filename = argv[5];
  if (argc >= 7) pf_filename = argv[6];
  if (argc >= 8) max_demotion_scans = atoi(argv[7]);

  parent_mask = numa_allocate_nodemask();
  if (!parent_mask) numa_error((char*) "numa_allocate_nodemask");
  numa_bitmask_setbit(parent_mask, 1);
  numa_set_membind(parent_mask); 
  
  if (run_kernel >= 0) {
    pid = getpid();
    pidfd = syscall(SYS_pidfd_open, pid, 0);
    smaps_filename = "/proc/" + to_string(pid) + "/smaps";
    stat_filename = "/proc/" + to_string(pid) + "/stat";
    if (access(done_filename, F_OK) != -1) remove(done_filename);

    // FORK #1: Create perf process
    int cpid = fork();
    if (cpid == 0) {
      numa_set_membind(parent_mask); 
      launch_perf(cpid, perf_cmd); // child process running perf
    } else {
      numa_set_membind(parent_mask);
      setpgid(cpid, 0); // set the child the leader of its process group

      node_addr = (unsigned long**) mmap(NULL, sizeof(unsigned long*), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
      edge_addr = (unsigned long**) mmap(NULL, sizeof(unsigned long*), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
      ret_addr = (unsigned long**) mmap(NULL, sizeof(unsigned long*), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
      ifstream nodes_edges_file(graph_fname + size_filename);
      nodes_edges_file >> num_nodes;
      nodes_edges_file >> num_edges;
      threshold = run_kernel*PERCENT_INTERVAL;

      // FORK #2: Create THP tracking process
      int cpid2 = fork();
      if (cpid2 == 0) {
        numa_set_membind(parent_mask);
        launch_thp_tracking(cpid2, run_kernel, thp_filename, pf_filename, max_demotion_scans); // child process tracking THPs
      } else {
        numa_set_membind(parent_mask);
        setpgid(cpid2, 0); // set the child the leader of its process group

        // Run app process
        launch_app(graph_fname, run_kernel, start_seed);

        // kill child processes and all their descendants, e.g. sh, perf stat, etc.
        kill(-cpid, SIGINT); // stop perf stat 
        //kill(-cpid2, SIGINT); // stop tracking THPs
   
        remove(done_filename);
      }
    }
  }

  return 0;
}
