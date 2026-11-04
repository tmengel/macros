#!/bin/bash
# Job list of the rho UE calibration from a list of DST files (e.g. HIJING DSTs, full paths),
# one job per file: <listdir>/<run>_<segment>.list and <listdir>/todo.txt for rhouecalib.job.
# The segment is the number before ".root" in the file name.
#   make_todo_files.sh <file list> <listdir> <run number>
if [ $# -lt 3 ]; then
  echo "usage: make_todo_files.sh <file list> <listdir> <run number>"
  exit 1
fi
FILES=$(readlink -f "$1")
LISTDIR=$2
RUN=$3
mkdir -p "${LISTDIR}"
LISTDIR=$(readlink -f "${LISTDIR}")
rm -f "${LISTDIR}"/*.list "${LISTDIR}/todo.txt"
n=0
while read -r file; do
  [ -n "${file}" ] || continue
  seg=$(echo "${file}" | sed -E 's/.*-([0-9]+)\.root/\1/')
  list="${LISTDIR}/${RUN}_${seg}.list"
  echo "${file}" > "${list}"
  echo "${RUN} ${seg} ${list}" >> "${LISTDIR}/todo.txt"
  n=$((n + 1))
done < "${FILES}"
echo "${n} jobs -> ${LISTDIR}/todo.txt"
