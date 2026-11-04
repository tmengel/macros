#!/bin/bash
# The whole rho UE calibration chain of one dataset, unattended: submit the knee step, wait for
# its jobs, merge (knee table), submit the s-map step, wait, merge (s-map payload), submit the
# UE step(s), wait, merge (UE payload). Plain condor_submit per step; the wait polls the
# .done / .failed markers of the jobs. Run it in the background:
#   nohup run_chain.sh <outdir> <todo> [condor_submit variables...] > <outdir>/chain.log 2>&1 &
#   e.g. run_chain.sh $O $O/lists/todo.txt setup=/path/setup.sh
#        UE_VARIANTS="ue: ue_alljets:seedeta=5" run_chain.sh $O $O/lists/todo.txt
# UE_VARIANTS (default "ue:"): "<name>:<variables>" per UE step; "ue" writes to <outdir>, any
# other name to <outdir>/<name> (its own ue/, logs/, ue_all.root and rho_ue_payload.root).
# Failed jobs are reported and the step is merged from the finished ones (merge.sh FORCE=1).
# FROM_STEP=smap|ue: start there (the earlier products must exist in <outdir>).
# The setup=<script> variable is also used for the merges.
# RESUME_FIRST=1: the first step's jobs are already in the queue (e.g. after a restart of this
# script): wait for them instead of submitting. Stale .done / .failed markers of an earlier
# launch must be removed first (find <dir>/<step> -name '*.root.*' -delete).
set -o pipefail
if [ $# -lt 2 ]; then
  echo "usage: run_chain.sh <outdir> <todo> [condor_submit variables...]"
  exit 1
fi
OUTDIR=$(readlink -f "$1")
TODO=$(readlink -f "$2")
shift 2
VARS=("$@")
HERE=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
NJOBS=$(wc -l < "${TODO}")
UE_VARIANTS=${UE_VARIANTS:-"ue:"}

submit_and_wait()  # <step> <outdir> [variables...]
{
  local step=$1 dir=$2
  shift 2
  mkdir -p "${dir}/logs/${step}" "${dir}/${step}"
  if [ "${RESUME_FIRST:-0}" = 1 ] && [ "${NSTEP}" -eq 0 ]; then
    echo "$(date '+%F %T') ${step} -> ${dir}: already submitted, waiting"
  else
    echo "$(date '+%F %T') submit ${step} -> ${dir} ($*)"
    (cd "${HERE}" && condor_submit rhouecalib.job "step=${step}" "todo=${TODO}" "outdir=${dir}" "${VARS[@]}" "$@") || exit 1
  fi
  NSTEP=$((NSTEP + 1))
  local ndone=0 nfail=0
  while true; do
    ndone=$(find "${dir}/${step}" -name '*.root.done' | wc -l)
    nfail=$(find "${dir}/${step}" -name '*.root.failed' | wc -l)
    [ $((ndone + nfail)) -ge "${NJOBS}" ] && break
    sleep 120
  done
  echo "$(date '+%F %T') ${step}: ${ndone} done, ${nfail} failed of ${NJOBS}"
  [ "${nfail}" -gt 0 ] && find "${dir}/${step}" -name '*.root.failed' | head -20
  if [ "${ndone}" -eq 0 ]; then
    echo "$(date '+%F %T') ${step}: no finished job, stopping"
    exit 1
  fi
  sleep 30
  FORCE=1 "${HERE}/merge.sh" "${step}" "${dir}" 2>&1 | grep -v '^hadd' || exit 1
}
NSTEP=0
for v in "${VARS[@]}"; do
  case "${v}" in setup=*) export SETUP=${v#setup=} ;; esac
done
FROM_STEP=${FROM_STEP:-knee}

[ "${FROM_STEP}" = knee ] && submit_and_wait knee "${OUTDIR}"
[ "${FROM_STEP}" != ue ] && submit_and_wait smap "${OUTDIR}" "knee=${OUTDIR}/knee.txt"
for variant in ${UE_VARIANTS}; do
  name=${variant%%:*}
  extra=${variant#*:}
  dir=${OUTDIR}
  [ "${name}" != "ue" ] && dir=${OUTDIR}/${name}
  # shellcheck disable=SC2086
  submit_and_wait ue "${dir}" "knee=${OUTDIR}/knee.txt" "smap=${OUTDIR}/smap_payload.root" ${extra}
done
echo "$(date '+%F %T') chain done: ${OUTDIR}"
