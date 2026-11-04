#!/bin/bash
# Merge the job outputs of one step and make the input of the next step:
#   merge.sh knee <outdir>   -> <outdir>/knee_all.root, <outdir>/knee.txt, <outdir>/knee_cdb/ (per-run knee payloads)
#   merge.sh smap <outdir>   -> <outdir>/smap_all.root, <outdir>/smap_payload.root
#   merge.sh ue   <outdir>   -> <outdir>/ue_all.root,   <outdir>/rho_ue_payload.root
# Stops if a job of the step failed or has not finished (set FORCE=1 to merge anyway).
# The builders run with their default parameters; run the make_*.C macros by hand for others.
# SETUP=<script>: sourced first (e.g. the setup with a local build of rhouecalib); default the "new" build.
STEP=$1
OUTDIR=$(readlink -f "$2")
if [ -n "${SETUP}" ] && [ "${SETUP}" != "none" ]; then
  source "${SETUP}" > /dev/null
else
  source /opt/sphenix/core/bin/sphenix_setup.sh -n new > /dev/null
fi
MACRODIR=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/..
case "${STEP}" in
  knee) MACRO=make_knee_table.C; PRODUCT=knee.txt ;;
  smap) MACRO=make_tower_shape_payload.C; PRODUCT=smap_payload.root ;;
  ue) MACRO=make_rho_ue_payload.C; PRODUCT=rho_ue_payload.root ;;
  *)
    echo "usage: merge.sh <knee|smap|ue> <outdir>"
    exit 1
    ;;
esac
DIR=${OUTDIR}/${STEP}
ndone=$(find "${DIR}" -name '*.root.done' | wc -l)
nfailed=$(find "${DIR}" -name '*.root.failed' | wc -l)
nfiles=$(find "${DIR}" -name "${STEP}_*.root" | wc -l)
echo "step ${STEP}: ${ndone} jobs done, ${nfailed} failed, ${nfiles} output files"
if [ "${FORCE:-0}" != 1 ] && { [ "${nfailed}" -gt 0 ] || [ "${ndone}" -ne "${nfiles}" ]; }; then
  echo "not merging: failed or unfinished jobs (FORCE=1 merges the finished ones)"
  exit 1
fi
if [ "${ndone}" -eq 0 ]; then
  echo "nothing to merge"
  exit 1
fi
find "${DIR}" -name '*.root.done' | sed 's/\.done$//' | sort > "${OUTDIR}/${STEP}_all.txt"
hadd -f -j 8 "${OUTDIR}/${STEP}_all.root" @"${OUTDIR}/${STEP}_all.txt" || exit 1
cd "${OUTDIR}" || exit 1
root -l -b -q "${MACRODIR}/${MACRO}(\"${OUTDIR}/${STEP}_all.root\", \"${OUTDIR}/${PRODUCT}\")" || exit 1
echo "-> ${OUTDIR}/${PRODUCT}"
if [ "${STEP}" = knee ]; then
  root -l -b -q "${MACRODIR}/make_knee_payloads.C(\"${OUTDIR}/knee.txt\", \"${OUTDIR}/knee_cdb\")" || exit 1
  echo "-> ${OUTDIR}/knee_cdb"
fi
