#!/bin/bash
# Job list of the rho UE calibration from a list of DST files (e.g. HIJING DSTs, full paths),
# one job per file: <listdir>/<run>_<segment>.list and <listdir>/todo.txt for rhouecalib.job.
# The segment label of a job is that of its first file.
#   make_todo_files.sh <file list> <listdir> <run number> [files per job, default 1]
# Choose the files per job so that a job runs about 30 minutes.
if [ $# -lt 3 ]; then
  echo "usage: make_todo_files.sh <file list> <listdir> <run number> [files per job]"
  exit 1
fi
FILES=$(readlink -f "$1")
LISTDIR=$2
RUN=$3
PERJOB=${4:-1}
mkdir -p "${LISTDIR}"
LISTDIR=$(readlink -f "${LISTDIR}")
find "${LISTDIR}" -maxdepth 1 \( -name '*.list' -o -name 'todo.txt' \) -delete
n=0
njobs=0
list=""
while read -r file; do
  [ -n "${file}" ] || continue
  if [ $((n % PERJOB)) -eq 0 ]; then
    # segment: the number before ".root", or before a trailing "-<n>.root" (embedding DSTs)
    seg=$(echo "${file}" | sed -E 's/.*-([0-9]{4,})-[0-9]+\.root$/\1/; t; s/.*-([0-9]+)\.root$/\1/')
    list="${LISTDIR}/${RUN}_${seg}.list"
    : > "${list}"
    echo "${RUN} ${seg} ${list}" >> "${LISTDIR}/todo.txt"
    njobs=$((njobs + 1))
  fi
  echo "${file}" >> "${list}"
  n=$((n + 1))
done < "${FILES}"
echo "${n} files in ${njobs} jobs -> ${LISTDIR}/todo.txt"
