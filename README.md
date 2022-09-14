
# The Implications of Page Size Management on Graph Analytics

## Applications

 1. **Breadth First Search (BFS)** - Given a starting (root) vertex, determine the minimum number of hops to all vertices. 

	In addition to its direct use in network analysis, e.g. LinkedIn degree separation, this algorithm also forms the basic building block of many other graph applications such as Graph Neural Networks, Connected Components, and Betweenness Centrality.
 2. **Single-Source Shortest Paths (SSSP)** - Given a starting (root) vertex, determine the minimum distance (sum of edge weights) to all vertices. 

	This algorithm is utilized in navigation and transportation problems as well as network utilization and its more general form is the $k$-shortest paths algorithm. 
 3. **PageRank (PR)** - Determine the "rank" or importance of all vertices (e.g. pages), where vertex scores are distributed to outgoing neighbors and updated until all scores converge, i.e. change by less than a threshold $\epsilon$. 

	Variants of this algorithm are used in ranking algorithms, e.g. of webpages, keywords, etc. 

The source codes for these applications are located in `launch/[APP NAME]`. These implementations are based on those from the [GraphIt framework](https://graphit-lang.org/).

## Datasets
We used the following datasets and their reordered variants:
1. **Kronecker_25** - synthetic power-law network
2. **Twitter** - real social network
3. **Sd1_Arc** - real web network
4. **Wikipedia** - real social network

These datasets are stored in [Compressed Sparse Row (CSR) Format](https://en.wikipedia.org/wiki/Sparse_matrix) as binary files. Each dataset has the following files:

 - `num_nodes_edges.text` stores the number of vertices and edges in the network, which is used to determine the amount of data to dynamically allocate for the graph application before it is populated with values
 - `node_array.bin` stores values in the *vertex array*, which are the cumulative number of neighbors each vertex has
 - `edge_array.bin` stores values in the *edge array*, which are the neighbor IDs for each vertex (this array is indexed by the vertex array) 
 - `edge_values.bin` stores values in the *values array*, which are edge weights for the path to each vertex's neighbor, if such weights exist

We use the Kronecker network generator from the [GAP Benchmark Suite](http://gap.cs.berkeley.edu/benchmark.html) and the real-world networks are from [SuiteSparse](https://sparse.tamu.edu/) and [SNAP](http://snap.stanford.edu/).

All dataset files are available [here](https://decades.cs.princeton.edu/datasets/big/). They can be downloaded via `wget`. See instructions below ("Data Setup") on how to set up data. 

### Degree-Based Grouping (DBG)

We perform dataset preprocessing as a standalone, separate step and store the preprocessed datasets as binary files as well. The code to perform the preprocessing and generate the dataset files is available at `utils/dbg.cpp`. It takes in a dataset folder (storing the 4 files described above) and outputs a file `dbg.txt` storing the preprocessed dataset in edgelist format:

    cd utils
    make
    ./dbg [PATH_TO_DATASET_FOLDER]

`PATH_TO_DATASET_FOLDER` is the path to the original dataset to be preprocessed. The edgelist file `dbg.txt` then needs to be converted to a binary file. This can be achieved with the following commands:

    cd graph_conversion
    make
    mkdir [PREPROCESSED_DATASET_FOLDER]
    ./edgelist_to_binary ../dbg.txt [PREPROCESSED_DATASET_FOLDER]

`PREPROCESSED_DATASET_FOLDER` is the name of the preprocessed dataset where the 4 dataset files, `num_nodes_edges.text`, `node_array.bin`, `edge_array.bin`, and `edge_values.bin`, for the resulting preprocessed CSR will be created. Once the creation of this new dataset folder is complete, `dbg.txt` can be removed and the folder can be moved to the `data/` folder (after performing data setup) where all other datasets are stored.

For details on the DBG algorithm, see the reference below.

**Reference**
Priyank Faldu, Jeff Diamond, and Boris Grot. 2020. [A Closer Look at Lightweight Graph Reordering](https://faldupriyank.com/papers/DBG_IISWC19.pdf). In *2019 IEEE International Symposium on Workload Characterization (IISWC)*. Institute of Electrical and Electronics Engineers (IEEE), United States, 1–13. 

## Experiments

### Prerequisites
 - Bash
 - Python3
 - Linux v5.15
 - Linux Perf
 - numactl (and NUMA support)
 - Root access

### NUMA Effects

In order to avoid NUMA latency effects, e.g. a combination of local and remote access latencies interfere with experiments, our experimental methodology is designed such that all application memory is allocated and used from one NUMA node, while all data is stored on another. Therefore, one node must be reserved for data storage. We used node 0 and  recommend using a node such that the applications can run on another node that is not node 0. Another separate node must be reserved for application execution.

### Data Setup

All data must be stored in tmpfs to eliminate page cache effects. This is done with the following commands:

    mkdir data
    sudo mount -t tmpfs -o size=100g,mpol=bind:[NUMA_NODE] tmpfs data
	cp -r [PATH_TO_DATASETS]/*Kronecker_25 [PATH_TO_DATASETS]/*Twitter [PATH_TO_DATASETS]/*Sd1_Arc [PATH_TO_DATASETS]/*Wikipedia data/
 
Where `NUMA_NODE` is the NUMA node where tmpfs data is pinned. 

### Experimental Setup

First, the experiment directory needs to be configured. Modify line 8 in `go.py` to specify the directory (full path) where this repository is cloned:

    HOME = "/home/aninda/vldb/"  # ADD DIRECTORY PATH HERE

Select a NUMA node to run the applications. This is the node where all application memory will be allocated and limited and/or fragmented. Then, modify line 10 in `measure.py`:

    NUMA_NODE = 1 # EDIT THIS VALUE (NUMA NODE)

### Scripts

There are several scripts that can be run individually to automate data collection. Below we detail how to run them: 

 - *go.py*: `sudo python3 go.py --experiment=[EXPERIMENT_NUMBER]  --dataset=[DATASET]  --app=[APP]  --config=[EXPERIMENT_CONFIG]`
 - *constrained.sh*: `sudo bash constrained.sh`
 - *frag.sh*: `sudo bash frag.sh [DATASET] [APP]`
 - *run_frag.sh*: `sudo bash run_frag.sh` 
 - *thp.sh*: `sudo bash thp.sh [EXPERIMENT_NUMBER] [DATASET] [APP] [CONFIG]`

We also provide utilities to constrain and fragment memory with kernel memory allocations. These scripts first need to be compiled. This can be achieved by running the following commands:

    cd numactl
    bash make.sh
    cd ../utils
    make
    cd ..

To run these utilities, we modified the Linux kernel source code to support system calls to allocate and free kernel memory. Our changes are available [here](https://github.com/amanocha/graphs_thp_linux).

We provide a program to constrain the amount of memory available to the application. It is a modified version of `memhog` from the `numactl` library and can be run as follows:

    numactl --membind [NUMA_NODE] ./numactl/memhog [MEMORY_TO_ALLOCATE_MB]M"

`NUMA_NODE` is the NUMA node selected for application memory allocations. `MEMORY_TO_ALLOCATE_MB` is the amount of memory in MB to use up on the NUMA node. For example, if 64GB are available on the node and 40GB (40960MB) are allocated via *memhog*, then only 24GB will be available to the application running on the node.

We provide one program to fragment memory, *fragm*, and one program to free memory, *free*. These programs are separated in order to be able to fragment memory and keep memory fragmented indefinitely while other processes are running. Because this fragmentation is performed via kernel memory allocations, it **must** be freed once the experiment is complete. The addresses of kernel memory allocations are written to a file *done.txt*, which is stored in the directory from where the program is invoked as follows:

    ./utils/fragm fragment [NUMA_NODE] [PAGE_ORDER] [FRAG_LEVEL]

`PAGE_ORDER` is always 9 because 2^9 4KB pages make up a huge page. `NUMA_NODE` is the NUMA node selected for application memory allocations. `FRAG_LEVEL` is expressed as the percentage of memory to fragment, e.g. `50`.

To free these allocations, run the following:

    ./utils/free [PATH_TO_DONE_FILE] [PAGE_ORDER] [NUMA_NODE]

`PATH_TO_DONE_FILE` is the path to where *done.txt* is located. If the fragmentation program was run from the *utils/* folder, then the path would simply be `done.txt`.

We have provided ways to run specific experiments and analyses and we suggest using these ways, described below. Each experiment is numbered and this numbering can be viewed in `go.py`. All commands must be run with `sudo` in order to be able to control THP settings. 

### TLB Miss Rates
To characterize the impacts of TLB misses and address translation overheads on graph analytics, run the following command:

    sudo bash thp.sh 1 all all

This command will run a sweep through all applications and datasets without (4KB base pages only) and with Linux THP enabled system-wide to measure TLB miss rates and application runtimes. The output will be stored in the `results/tlb_char/` folder.

### Data Structure Analysis

To characterize the performance impacts of utilizing huge pages for individual data structures in graph analytics, namely the vertex array, edge array, and property array, run the following command:

    sudo bash thp.sh 2 all all

This command will run a sweep through all applications and datasets with Linux THP enabled for only one region of memory corresponding to one of the data structures. This selective THP usage is performed via the `madvise` system call. The output will be stored in the `results/data_struct/` folder.

### Constrained Memory

To characterize the performance impacts of memory pressure on Linux THP performance, the scripting environment must be first be configured. Open the file `constrained.sh` in a text editor and modify lines 7 and 8 based on the NUMA node ID and the amount of memory available (in MB) on the node (e.g. NUMA node 1 has 64GB of RAM):

    NUMA_NODE=1  # EDIT THIS VALUE (NUMA NODE)
    MAX_RAM=64000  # EDIT THIS VALUE (AMOUNT OF MEMORY ON NUMA NODE)

Then run the following:

    sudo bash constrained.sh

This command will run a sweep through all applications and datasets without (4KB base pages only) and with Linux THP enabled system-wide in the presence of different amounts of memory pressure, i.e. 0-3 additional GB in increments of 512MB, plus 512MB of oversubscription. This is to measure the impacts of memory pressure on Linux THP performance. The output will be stored in the `results/constrained_mem/` folder.

### Fragmented Memory

To characterize the performance impacts of memory fragmentation on Linux THP performance, the scripting environment must be first be configured. Open the file `frag.sh` in a text editor and modify lines 9 and 10 based on the NUMA node ID and the amount of memory available (in MB) on the node (e.g. NUMA node 1 has 64GB of RAM):

    NUMA_NODE=1  # EDIT THIS VALUE (NUMA NODE)
    MAX_RAM=64000  # EDIT THIS VALUE (AMOUNT OF MEMORY ON NUMA NODE)
    
Then run the following:

    sudo bash run_frag.sh 4

This command will run a sweep through all applications and datasets without (4KB base pages only) and with Linux THP enabled system-wide in the presence of different amounts of memory fragmentation, i.e. 0-100% in increments of 25%. For all experiments, there are 3 additional GB available relative to the application's working set size. This is to measure the impacts of memory pressure on Linux THP performance. The output will be stored in the `results/frag_mem/` folder.

### Selective THP

To characterize the performance impacts of selective THP usage, run the following:

    sudo bash run_frag.sh 5

This command will run a sweep through all applications and datasets (both original and DBG preprocessed) with 4KB base pages only, with Linux THP enabled system-wide, and with THP applied to different percentages (10-100% in increments of 10%) of the property array. For all experiments, there are 3 additional GB available relative to the application's working set size and memory is 50% fragmented. This is to measure the performance benefits of selectively using THP where data is known to be hot, i.e. the top 10-20% of the property array when the dataset is preprocessed, in the presence of memory pressure. The output will be stored in the `results/select_thp/` folder.

## Results

If the experiment scripts were used, all results are stored in the `results/` folder. Within each experiment type, e.g. `tlb_char`, `data_struct`, etc., there may be folders for different configurations, e.g. `vertex_array`, `edge_array`, etc, and then folders for the baseline (`none`) vs. Linux THP (`thp`). Within these folders, experiment results are organized by application and dataset combination. The folder organization can be summarized as follows:

    - tlb_char
	    - none
	    - thp
    - data_struct
	    - none
	    - thp
	    - vertex_array
	    - edge_array
	    - prop_array
    - constrained_mem
	    - 3GB
		    - none
		    - thp
		    - none_flipped
		    - thp_flipped
	    - ...
	    - -0.5GB
		    - none
		    - thp
		    - none_flipped
		    - thp_flipped
    - frag_mem
	    - 0
		    - none
		    - thp
		    - none_flipped
		    - thp_flipped
	    - 25
		    - none
		    - thp
		    - none_flipped
		    - thp_flipped
	    - 50
		    - none
		    - thp
		    - none_flipped
		    - thp_flipped
	    - 75
		    - none
		    - thp
		    - none_flipped
		    - thp_flipped
    - select_thp
	    - none
	    - thp
	    - thp_10
	    - ...
	    - thp_100

Within each application/dataset experiment folder, the following files are generated:

    - compiler_output.txt (compilation standard output)
    - compiler_err.txt (compilation standard error output)
    - app_output_x_i.txt (application standard output)
    - err_output_x_i.txt (application standard error output)
    - measurements_output_i.txt (metric values from perf)
    - perf_output_x_i.txt (perf output)
    - pf_output_x_i.txt (page faults over time)
    - results_output_i.txt (results output)
    - results.txt (average results output)
    - thp_output_x_i.txt (number of huge pages over time)

`x` represents the THP setting (e.g. 0 for baseline and THP, 2 for 10% of the property array, etc.) and `i` is the iteration number (each experiment is run 3 times). 

The runtimes for a given execution will be at the bottom of `app_output_x_i.txt` and appear as follows:

    total kernel computation time: [TIME_IN_SECONDS]
    user time: [USER_TIME_IN_SECONDS]
    kernel time: [KERNEL_TIME_IN_SECONDS]

The TLB miss rates for a given execution will be at the top of `results_i.txt` and appear as follows:

    TLB:
    TLB Miss Rate: [%]
    STLB Miss Rate: [%]
    Page Fault Rate: [%]
    Percent of TLB Accesses with PT Walks: [%]
    Percent of TLB Accesses with Completed PT Walks: [%]
    Percent of TLB Accesses with Page Faults: [%]

The average TLB statistics will be recorded in `results.txt`.

## Contact
Aninda Manocha: amanocha@princeton.edu
