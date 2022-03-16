import argparse
import numpy as np
import os
import re
import sys
import time

from go import *

NUMA_NODE = 1 # EDIT THIS VALUE (NUMA NODE)
FREQ = 2.1e9

# EXPERIMENT INFO
output = ""

# FILE INFO
source = ""
data = ""
relative = 0

# COMPILER AND EXECUTION INFO
filename = ""
app_name = ""
app_input = ""
new_dir = ""
start_seed = 0
EXEC = "main"

# METRICS
general = ["instructions", "cpu-cycles"]
l1cache = ["L1-dcache-loads", "L1-dcache-load-misses", "L1-dcache-stores"]
llc = ["LLC-loads", "LLC-load-misses", "LLC-stores", "LLC-store-misses"]
virtual = [
            "dTLB-loads", "dTLB-stores", "page-faults",
            "dtlb_load_misses.stlb_hit", "dtlb_load_misses.miss_causes_a_walk",
            "dtlb_load_misses.walk_completed", "dtlb_load_misses.walk_completed_1g", "dtlb_load_misses.walk_completed_2m_4m", "dtlb_load_misses.walk_completed_4k",
            "dtlb_store_misses.stlb_hit", "dtlb_store_misses.miss_causes_a_walk",
            "dtlb_store_misses.walk_completed", "dtlb_store_misses.walk_completed_1g", "dtlb_store_misses.walk_completed_2m_4m", "dtlb_store_misses.walk_completed_4k"
          ]
metrics = general + l1cache + llc + virtual
metric_vals = {}

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-x", "--num_samples", type=int, default=5, help="Number of experiment samples")
    parser.add_argument("-s", "--source", type=str, help="Path to source code file")
    parser.add_argument("-d", "--data", type=str, help="Path to data file")
    parser.add_argument("-rp", "--relative_path", type=int, default=0, help="Use relative data file path")
    parser.add_argument("-ss", "--start_seed", type=int, default=-1, help="Start seed for BFS and SSSP")
    parser.add_argument("-o", "--output", type=str, help="Output path")
    parser.add_argument("-ma", "--madvise", type=str, default="1", help="Madvise mode")
    args = parser.parse_args()
    return args

def compile():
    print("Compiling application...\n")
    compile_mode = "db"
    cmd_args = ["g++", "-O3", "-o", EXEC, "-std=c++11", "-lnuma"]
    cmd_args += [filename, ">", output + "compiler_output.txt", "2>", output + "compiler_err.txt"]
    cmd = " ".join(cmd_args)
    print(cmd)
    os.system(cmd)

def execute(run_kernel, iteration=""):
    print("Executing application...")
    if (relative):
        input_path = os.path.relpath(data, new_dir)
    else:
        input_path = data

    perf_name = "perf_output_" + run_kernel + "_" + iteration + ".txt" if iteration != "" else "perf_output_" + run_kernel + ".txt"
    if (os.path.isfile(output + perf_name)):
        return

    app_name = "app_output_" + run_kernel + "_" + iteration + ".txt" if iteration != "" else "app_output_" + run_kernel + ".txt"
    err_name = "err_output_" + run_kernel + "_" + iteration + ".txt" if iteration != "" else "err_output_" + run_kernel + ".txt"
    thp_name = "thp_output_" + run_kernel + "_" + iteration + ".txt" if iteration != "" else "thp_output_" + run_kernel + ".txt"
    pf_name = "pf_output_" + run_kernel + "_" + iteration + ".txt" if iteration != "" else "pf_output_" + run_kernel + ".txt"

    cmd_args = ["\"perf", "stat", "-p", "%d", "-B", "-v", "-o", perf_name]
    for metric in metrics:
        cmd_args += ["-e", metric + ":u"]
    perf_cmd = " ".join(cmd_args) + "\""
    
    cmd_args = ["numactl -C 0 --membind=" + str(NUMA_NODE) + " sudo ./" + EXEC, input_path, run_kernel]

    if start_seed != -1:
        cmd_args += [str(start_seed)]

    cmd_args += [perf_cmd, thp_name, pf_name]
    cmd_args += [">", app_name, "2>", err_name]

    cmd = " ".join(cmd_args)
    print(cmd)
    os.system(cmd)
    
    time.sleep(5)

    os.system("mv " + perf_name + " " + output)
    os.system("mv " + app_name + " " + output)
    os.system("mv " + err_name + " " + output)
    os.system("mv " + thp_name + " " + output)
    os.system("mv " + pf_name + " " + output)

def measure(run_kernel, iteration=""):
    global metric_vals

    print("Gathering measurements...")
    filename = output + "perf_output_" + run_kernel + "_" + iteration + ".txt" if iteration != "" else output + "perf_output_" + run_kernel + ".txt"
    if (not os.path.isfile(filename)):
        return
    perf_output = open(filename, "r+")
    measurements = open(output + "measurements_" + iteration + ".txt", "w+")

    for line in perf_output:
      line = line.replace(",","").replace("-", "")
      match1 = re.match("\s*(\d+)\s+(\w+\.?\w*\.?\w*\.?\w*\.?\w*)", line)

      if match1 != None:
        metric = match1.group(2)
        value = int(match1.group(1))
      
        if metric in metric_vals:
          metric_vals[metric] += value
        else:
          metric_vals[metric] = value
        print(metric, value)
        measurements.write(metric + ": " + str(value) + "\n")
      else:
        metric = ""
        value = 0

    perf_output.close()
    measurements.close()
    print("\n")

def avg(N):
    filename = output + "measurements.txt"
    if (not os.path.isfile(filename)):
        return
    measurements = open(filename, "w+")

    measurements.write("AVERAGES:\n")
    measurements.write("---------\n")
    for metric in metric_vals:
      metric_vals[metric] = round(float(metric_vals[metric])/N, 2)
      measurements.write(metric + ": " + str(metric_vals[metric]) + "\n")

    measurements.close()

def parse_results(run_kernel, iteration=""):
    # Parse Cache Measurements
    l1_refs = metric_vals["L1dcacheloads"] + metric_vals["L1dcachestores"]
    l1_misses = metric_vals["L1dcacheloadmisses"] #+ metric_vals["L1dcachestoremisses"]

    llc_refs = metric_vals["LLCloads"] + metric_vals["LLCstores"]
    llc_misses = metric_vals["LLCloadmisses"] + metric_vals["LLCstoremisses"]

    # Parse TLB Measurements
    tlb_refs = metric_vals["dTLBloads"] + metric_vals["dTLBstores"]
    tlb_stlb = metric_vals["dtlb_load_misses.stlb_hit"] + metric_vals["dtlb_store_misses.stlb_hit"]
    tlb_walks = metric_vals["dtlb_load_misses.miss_causes_a_walk"] + metric_vals["dtlb_store_misses.miss_causes_a_walk"]
    tlb_misses = tlb_stlb + tlb_walks

    tlb_walk_completed = metric_vals["dtlb_load_misses.walk_completed"] + metric_vals["dtlb_store_misses.walk_completed"]
    tlb_walk_completed_4k = metric_vals["dtlb_load_misses.walk_completed_4k"] + metric_vals["dtlb_store_misses.walk_completed_4k"]
    tlb_walk_completed_2m = metric_vals["dtlb_load_misses.walk_completed_2m_4m"] + metric_vals["dtlb_store_misses.walk_completed_2m_4m"]
    tlb_walk_completed_1g = metric_vals["dtlb_load_misses.walk_completed_1g"] + metric_vals["dtlb_store_misses.walk_completed_1g"]
    tlb_walk_completed_tot = tlb_walk_completed_4k + tlb_walk_completed_2m + tlb_walk_completed_1g

    page_faults = metric_vals["pagefaults"]

    # Parse Pipeline Measurements
    instructions = metric_vals["instructions"]
    cycles = metric_vals["cpucycles"]
    memory_ops = l1_refs
    compute_ops = instructions - memory_ops
    ratio = compute_ops/memory_ops
    avg_bw = llc_misses*64*4/(1024*1024*1024)/(cycles/FREQ)
    ipc = float(instructions)/cycles
    
    # Write Results
    if iteration != "":
      results_file = "results_" + iteration + ".txt"
      measurements_file = "measurements_" + iteration + ".txt"
    else:
      results_file = "results.txt"
      measurements_file = "measurements.txt"

    measurements = open(output + results_file, "w+")

    measurements.write("RESULTS:\n")
    measurements.write("----------\n")
    measurements.write("\nCACHE:\n")
    measurements.write("L1 Miss Rate: " + str(l1_misses*100.0/l1_refs) + "\n")
    measurements.write("LLC Miss Rate: " + str(llc_misses*100.0/llc_refs) + "\n")

    measurements.write("\nTLB:\n")
    if (tlb_refs > 0):
      measurements.write("TLB Miss Rate: " + str(tlb_misses*100.0/tlb_refs) + "\n")
    if (tlb_walks > 0):
      measurements.write("STLB Miss Rate: " + str(tlb_walks*100.0/(tlb_walks+tlb_stlb)) + "\n")
      measurements.write("Page Fault Rate: " + str(page_faults*100.0/tlb_walks) + "\n")
    if (tlb_refs > 0):
      measurements.write("Percent of TLB Accesses with PT Walks: " + str(tlb_walks*100.0/tlb_refs) + "\n")
      measurements.write("Percent of TLB Accesses with Completed PT Walks: " + str(tlb_walk_completed*100.0/tlb_refs) + "\n")
      measurements.write("Percent of TLB Accesses with Page Faults: " + str(page_faults*100.0/tlb_refs) + "\n")
    if (tlb_walk_completed_tot > 0):
      measurements.write("4KB Page Table Walks: " + str(tlb_walk_completed_4k*100.0/tlb_walk_completed_tot) + "\n")
      measurements.write("2MB/4MB Page Table Walks: " + str(tlb_walk_completed_2m*100.0/tlb_walk_completed_tot) + "\n")
      measurements.write("1GB Page Table Walks: " + str(tlb_walk_completed_1g*100.0/tlb_walk_completed_tot) + "\n")

    measurements.write("\nPIPELINE:\n")
    measurements.write("Cycles: " + str(cycles) + "\n")
    if (cycles > 0):
      measurements.write("Calculated IPC: " + str(ipc) + "\n")
    measurements.write("Calculated Compute to Memory Ratio: " + str(ratio) + "\n")
    measurements.write("Average BW: " + str(avg_bw) + "\n")
    measurements.write("Total Number of Memory Instructions: " + str(round(tlb_refs,3)) + "\n")
    measurements.write("Percent of Instructions Spent on Memory: " + str(round(tlb_refs*100.0/instructions,3)) + "\n")

    measurements.close()

def main():
    global source, data, relative, output, start_seed
    global filename, app_name, app_input, new_dir

    args = parse_args()

    if not os.path.isfile(args.source):
        print("Invalid file path entered!\n")
    else:
	# Parse Arguments
        source = args.source
        data = args.data
        relative = args.relative_path
        start_seed = args.start_seed
        num_samples = args.num_samples
        run_kernel = args.madvise

	# Set Up Experiment Information
        filename = os.path.basename(source)
        app_name = source.split("/")[0]
        names = data.split('/')
        app_input = names[len(names)-2]
        new_dir = os.path.dirname(source)

        if args.output:
            output = args.output
        else:
            output = "results/all/" + app_name + "_" + app_input + "/"

        if (not os.path.isdir(output)):
            mkdirp(output)
        
        os.chdir(new_dir)

        print("Application: " + app_name)
        print("Application file path: " + source)
        print("Application input: " + app_input)
        print("Output directory: " + output)
        print("------------------------------------------------------------\n")
        
	# Run Experiment
        start_time = time.time()

        compile()
        for i in range(num_samples):
            execute(run_kernel, str(i))
            measure(run_kernel, str(i))
            parse_results(run_kernel, str(i))
        avg(num_samples)
        parse_results(run_kernel)
        
        end_time = time.time()
        print("Total Time = " + str(round(end_time - start_time)) + " seconds.\n")

if __name__ == "__main__":
    main()
