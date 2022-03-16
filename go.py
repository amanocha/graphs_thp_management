import argparse
import math
import os
import sys

from subprocess import Popen, PIPE

HOME = "/home/aninda/vldb/" # ADD DIRECTORY PATH HERE
GRAPH_DIR = HOME + "data/"
RESULT_DIR = HOME + "results/"

# APPS
apps = ["bfs", "sssp", "pagerank"]

# INPUTS
datasets = ["Kronecker_25", "Twitter", "Sd1_Arc", "Wikipedia"]
vp_inputs = []

inputs = {
         "bfs": vp_inputs,
         "sssp": vp_inputs,
         "pagerank": vp_inputs,
       }

start_seed = {"Kronecker_25/": "0", "Twitter/": "0", "Sd1_Arc/": "0", "Wikipedia/": "0",
	      "DBG_Kronecker_25/": "3287496", "DBG_Twitter/": "15994127", "DBG_Sd1_Arc/": "18290613", "DBG_Wikipedia/": "320944"}

NUM_ITER = 3

def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument("-a", "--app", type=str, help="Application to run (bfs, sssp, pagerank)")
  parser.add_argument("-d", "--dataset", type=str, help="Dataset to run")
  parser.add_argument("-x", "--experiment", type=int, default=-1, help="Experiment to run (0-4)")
  parser.add_argument("-c", "--config", type=str, help="Experiment configuration")
  args = parser.parse_args()
  return args

def run(cmd, tmp_output, output):
  print(cmd)
  exit = os.system(cmd)
  if not exit:
    if tmp_output != output:
      os.system("cp -r " + tmp_output + " " + output)
      os.system("rm -r " + tmp_output)
    print("Done! Navigate to " + output + "results.txt to see the results!")
  else:
    print("Experiment failed!")

# The function below is from https://csatlas.com/python-create-directory/
def mkdirp(path):
  try:
    os.makedirs(path)
  except OSError:
    if not os.path.isdir(path):
      raise

def setup_run(madvise, exp_type, flipped=False):
  exp_dir = RESULT_DIR + exp_type + "/"
  if not os.path.isdir(exp_dir):
    mkdirp(exp_dir)
  for app in apps:
    source = "launch/" + app + "/main.cpp"
    if flipped:
      source = "launch/" + app + "/flipped.cpp"
    for input in inputs[app]:
      names = input.split("/")
      output = app + "_" + names[len(names)-2] + "/"
      if not os.path.isdir(exp_dir + output):
        tmp_output = exp_dir + "tmp_" + output
        data = GRAPH_DIR + input
        cmd_args = ["time python3 measure.py", "-s", source, "-d", data, "-o", tmp_output, "-ma", str(madvise), "-x", str(NUM_ITER)]
        if "bfs" in app or "sssp" in app:
          cmd_args += ["-ss", start_seed[input]]
        cmd = " ".join(cmd_args)
        run(cmd, tmp_output, exp_dir + output)

# Experiment 1: TLB Characterization
def run_tlb_char():
  exp_type = "tlb_char/"
  if is_thp == 1:
    exp_type += "thp"
  elif is_thp == 2:
    exp_type += "thp_no_defrag"
  else:
    exp_type += "none"
  setup_run(0, exp_type)
 
# Experiment 2: Data Structure Analysis
def run_data_struct():
  default = "thp" if is_thp == 1 else "none"
  end = 400 if is_thp == 0 else 1
  for madvise in range(0, end, 100):
    exp_type = "data_struct/"
    if madvise == 100:
      exp_type += "prop_array"
    elif madvise == 200:
      exp_type += "vertex_array"
    elif madvise == 300:
      exp_type += "edge_array"
    elif madvise == 400:
      exp_type += "values_array"
    else:
      exp_type += default
    setup_run(madvise, exp_type)

#Experiment 3: Constrained Memory
def run_constrained_mem():
  for f in range(2):
    exp_type = "constrained_mem/"
    if args.config:
      exp_type += args.config + "/" 
    if is_thp == 1:
      exp_type += "thp"
    elif is_thp == 2:
      exp_type += "thp_no_defrag"
    else:
      exp_type += "none"
    if f == 1:
      exp_type += "_flipped"
    setup_run(0, exp_type, f)

#Experiment 4: Fragmented Memory
def run_frag_mem():
  for f in range(2):
    exp_type = "frag_mem/"
    if args.config:
      exp_type += args.config + "/"
    if is_thp == 1:
      exp_type += "thp"
    elif is_thp == 2:
      exp_type += "thp_no_defrag"
    else:
      exp_type += "none"
    if f == 1:
      exp_type += "_flipped"
    setup_run(0, exp_type, f)

#Experiment 5: Selective THP
def run_select_thp():
  default = "thp" if is_thp == 1 else "none"
  end = 21 if is_thp == 0 else 1
  for madvise in range(0, end, 2):
    exp_type = "select_thp/"
    if madvise == 0:
      exp_type += default
    else:
      exp_type += "thp_" + str(madvise*5)
    setup_run(madvise, exp_type)

# EXPERIMENTS
experiments = {
                1: run_tlb_char,
                2: run_data_struct,
		3: run_constrained_mem,
		4: run_frag_mem,
		5: run_select_thp
              }

def main():
  global apps, args, is_thp, datasets, vp_inputs

  args = parse_args()

  # Get THP settings
  stdout = Popen("cat /sys/kernel/mm/transparent_hugepage/enabled", shell=True, stdout=PIPE).stdout
  output_str1 = stdout.read().decode("utf-8")
  stdout = Popen("cat /sys/kernel/mm/transparent_hugepage/defrag", shell=True, stdout=PIPE).stdout
  output_str2 = stdout.read().decode("utf-8")
  if (output_str1.startswith("[always]") and output_str2.startswith("[always]")):
    is_thp = 1
  elif (output_str1.startswith("[always]") and not output_str2.startswith("[always]")):
    is_thp = 2
  else:
    is_thp = 0

  # Apps
  if args.app and args.app != "all":
    if args.app not in apps:
      print("Invalid application!")
      sys.exit(1)
    apps = [args.app]

  # Inputs
  if args.dataset and args.dataset != "all":
    if args.dataset not in datasets:
      print("Invalid dataset!")
      sys.exit(1)
    datasets = [args.dataset]
    
  for dataset in datasets:
    vp_inputs += [dataset + "/"]
    if args.experiment == 5:
      vp_inputs += ["DBG_" + dataset + "/"]

  # Experiments 
  if args.experiment != -1:
    if args.experiment not in experiments:
      print("Invalid experiment!")
      sys.exit(1)
    experiments[args.experiment]()
  else:
    run_tlb_char()
    run_data_struct()
    run_constrained_mem()
    run_frag_mem()
  
    for dataset in datasets:
      vp_inputs += ["DBG_" + dataset + "/"]
    run_select_thp()

if __name__ == "__main__":
  main()
