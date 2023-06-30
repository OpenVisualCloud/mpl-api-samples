# Benchmark tool introduction
## 1.Overview
Benchmark tool consists of three scripts, including benchmark.config, benchmark.sh and extract.sh. The parameters of command line are set in benchmark.config. The script benchmark.sh contains command lines to benchmark the IMPL functions and pipelines. The script extract.sh is responsible for extracting the benchmark result after running benchmark.sh, generating a benchmark csv form.

## 1.Config benchmark tool
Some parameters in benchmark.config are illustrated as following:
| parameters           | illustration                                   |
| :---                 | :---                                           |
| frame_value          | the total frames amount                        |
| multi_process_number | the channels amount                            |
| resize               | 1: benchmark resize, 0: don't benchmark        |
| csc                  | 1: benchmark csc, 0: don't benchmark           |
| composition          | 1: benchmark composition, 0: don't benchmark   |
| alphablending        | 1: benchmark alphablending, 0: don't benchmark |

For each filter, benchmark.config records the formats and sizes need to be benchmarked, more formats and sizes can be added to the corresponding list, pls add corresponding input file directory as well.

## 2.Benchmark the filters
After setting parameters in benchmark.config, pls run the following commands in order:
```shell
source /opt/intel/oneapi/setvars.sh
./benchmark.sh
```
After running benchmark.sh, some folders ending with log will be produced.

## 3.Extract performance data
After running benchmark.sh, run extract.sh to extract the performance data from folders ending with log. A csv form named result.csv will be produced.
```shell
./extract.sh
```
