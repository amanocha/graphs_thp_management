# The Implications of Page Size Management on Graph Analytics

## Applications

 1. **Breadth First Search (BFS)** - Given a starting (root) vertex, determine the minimum number of hops to all vertices. 

	In addition to its direct use in network analysis, e.g. LinkedIn degree separation, this algorithm also forms the basic building block of many other graph applications such as Graph Neural Networks, Connected Components, and Betweenness Centrality.
 2. **Single-Source Shortest Paths (SSSP)** - Given a starting (root) vertex, determine the minimum distance (sum of edge weights) to all vertices. 

	This algorithm is utilized in navigation and transportation problems as well as network utilization and its more general form is the $k$-shortest paths algorithm. 
 3. **PageRank (PR)** - Determine the "rank" or importance of all vertices (e.g. pages), where vertex scores are distributed to outgoing neighbors and updated until all scores converge, i.e. change by less than a threshold $\epsilon$. 

	Variants of this algorithm are used in ranking algorithms, e.g. of webpages, keywords, etc. 

The source code for these applications are located in `launch/[APP NAME]`.

## Datasets
All datasets are available [here](https://decades.cs.princeton.edu/datasets/big/).

## Degree-Based Grouping (DBG)

For details on the algorithm, see the reference below.

**Reference**
Priyank Faldu, Jeff Diamond, and Boris Grot. 2020. [A Closer Look at Lightweight Graph Reordering](https://faldupriyank.com/papers/DBG_IISWC19.pdf). In *2019 IEEE International Symposium on Workload Characterization (IISWC)*. Institute of Electrical and Electronics Engineers (IEEE), United States, 1–13. 

## Contact
Aninda Manocha: amanocha@princeton.edu
