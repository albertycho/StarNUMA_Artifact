## Cloning full repo:
git clone --recursive https://github.com/arun-sub/genomicsbench.git

wget https://genomicsbench.eecs.umich.edu/input-datasets.tar.gz

Set env variable:
export GENOMICSBENCH_PATH=cloned_genomicsbench

Then run replace_files.sh.
If you get an error in ubuntu 22 or later, try replace_files_rdtscerr.sh instead.
The script should replace the necessary files and generate required binaries used in StarNUMA.
