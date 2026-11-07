#!/bin/bash
# Job list of the rho UE calibration from the file catalog: the DST_CALOFITTING segments of
# each run that also have the other DST types needed (DST_ZDC_RAW for the minimum-bias
# classifier), one job per segment by default.
#   make_todo.sh <tag> <runlist> <listdir> [segments per job, default 1] [other DST types, default "DST_ZDC_RAW"]
#   e.g. make_todo.sh pro001_pcdb001_v001 runs.txt /path/to/lists
# Writes <listdir>/<run>_<first segment>.list (DST_CALOFITTING names, resolved through the
# file catalog by Fun4All) and <listdir>/todo.txt (lines "run segment list") for rhouecalib.job.
if [ $# -lt 3 ]; then
  echo "usage: make_todo.sh <tag> <runlist> <listdir> [segments per job] [other DST types]"
  exit 1
fi
TAG=$1
RUNLIST=$(readlink -f "$2")
LISTDIR=$3
NSEG=${4:-1}
OTHER=${5:-"DST_ZDC_RAW"}
mkdir -p "${LISTDIR}"
LISTDIR=$(readlink -f "${LISTDIR}")
rm -f "${LISTDIR}"/*.list "${LISTDIR}/todo.txt"

WORK=$(mktemp -d)
cd "${WORK}" || exit 1
# shellcheck disable=SC2086
CreateDstList.pl --tag "${TAG}" --list "${RUNLIST}" DST_CALOFITTING ${OTHER} > /dev/null
nruns=0
nseg=0
for calo in dst_calofitting-*.list; do
  [ -f "${calo}" ] || continue
  run=$(echo "${calo}" | sed -E 's/dst_calofitting-0*([0-9]+)\.list/\1/')
  # segments (run-segment suffix) present in every DST type
  sed -E 's/.*-([0-9]+-[0-9]+)\.root/\1/' "${calo}" | sort > segs.txt
  for type in ${OTHER}; do
    other=$(echo "${type}" | tr '[:upper:]' '[:lower:]')-$(echo "${calo}" | sed -E 's/dst_calofitting-([0-9]+)\.list/\1/').list
    sed -E 's/.*-([0-9]+-[0-9]+)\.root/\1/' "${other}" 2> /dev/null | sort > other.txt
    comm -12 segs.txt other.txt > both.txt
    mv both.txt segs.txt
  done
  sort -t- -k2 segs.txt | while read -r rs; do grep -- "-${rs}.root" "${calo}"; done > both.txt
  nmissing=$(($(wc -l < "${calo}") - $(wc -l < both.txt)))
  [ ${nmissing} -gt 0 ] && echo "run ${run}: ${nmissing} DST_CALOFITTING segments without ${OTHER} skipped"
  [ -s both.txt ] || continue
  split -l "${NSEG}" -d -a 5 both.txt group_
  for g in group_*; do
    seg=$(head -1 "${g}" | sed -E 's/.*-([0-9]+)\.root/\1/')
    list="${LISTDIR}/${run}_${seg}.list"
    mv "${g}" "${list}"
    echo "${run} ${seg} ${list}" >> "${LISTDIR}/todo.txt"
  done
  nruns=$((nruns + 1))
  nseg=$((nseg + $(wc -l < both.txt)))
done
cd / && rm -rf "${WORK}"
echo "${nruns} runs, ${nseg} segments, $(wc -l < "${LISTDIR}/todo.txt" 2> /dev/null || echo 0) jobs -> ${LISTDIR}/todo.txt"
