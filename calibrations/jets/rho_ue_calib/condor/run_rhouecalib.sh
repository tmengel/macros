#!/bin/bash
# One rho UE calibration job (see rhouecalib.job):
#   run_rhouecalib.sh <step> <input list> <output file> <cdb tag> <knee table> <s-map payload> <setup> <macro dir> <macro> <veto> <refit> <seed |eta|> <ZS list> <centrality dir>
# "none" = not set, "window" = the default seed acceptance (-1). <setup>: script to source for the environment (default: the "new" build).
# <macro>: Fun4All_RhoUECalib.C (data) or Fun4All_RhoUECalib_Sim.C (HIJING; cdb tag, veto and
# refit unused; <centrality dir>: the simulation centrality scales, "none" = from the CDB). A marker
# <output>.done or <output>.failed records the result.
STEP=$1
LIST=$2
OUTFILE=$3
CDBTAG=$4
KNEE=$5
SMAP=$6
SETUP=$7
MACRODIR=$8
MACRO=${9:-Fun4All_RhoUECalib.C}
VETO=${10:-1}
REFIT=${11:-1}
SEEDETA=${12:-window}
[ "${SEEDETA}" = "window" ] && SEEDETA=-1
ZSLIST=${13:-none}
[ "${ZSLIST}" = "none" ] && ZSLIST=""
CENTDIR=${14:-none}
[ "${CENTDIR}" = "none" ] && CENTDIR=""
[ "${KNEE}" = "none" ] && KNEE=""
[ "${SMAP}" = "none" ] && SMAP=""
if [ "${SETUP}" = "none" ]; then
  source /opt/sphenix/core/bin/sphenix_setup.sh -n new
else
  source "${SETUP}"
fi
echo "rho UE calibration, step ${STEP}, ${MACRO}: $(wc -l < "${LIST}") files from ${LIST} -> ${OUTFILE}"
mkdir -p "$(dirname "${OUTFILE}")"
rm -f "${OUTFILE}.done" "${OUTFILE}.failed"
# ROOT and Fun4All scratch files stay in the job's own directory
cd "${_CONDOR_SCRATCH_DIR:-$(mktemp -d)}" || exit 1
case "${MACRO}" in
  *_Sim.C)
    ARGS="0, \"${LIST}\", \"${OUTFILE}\", \"${STEP}\", \"${KNEE}\", \"${SMAP}\", ${SEEDETA}, \"MDC2\", 31, \"${CENTDIR}\"" ;;
  *)
    VETOB=false; [ "${VETO}" = 1 ] && VETOB=true
    REFITB=false; [ "${REFIT}" = 1 ] && REFITB=true
    ARGS="0, \"${LIST}\", \"${OUTFILE}\", \"${STEP}\", \"${CDBTAG}\", \"${KNEE}\", \"${SMAP}\", ${VETOB}, ${REFITB}, ${SEEDETA}, \"${ZSLIST}\"" ;;
esac
root -l -b -q "${MACRODIR}/${MACRO}(${ARGS})"
EXITCODE=$?
if [ ${EXITCODE} -eq 0 ] && [ -s "${OUTFILE}" ]; then
  touch "${OUTFILE}.done"
else
  echo "exit ${EXITCODE}" > "${OUTFILE}.failed"
  echo "Error: step ${STEP} failed with exit code ${EXITCODE}"
fi
exit ${EXITCODE}
