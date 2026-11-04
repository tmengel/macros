// Rho UE calibration, after step "knee": fit the MBD charge knee of every run in the
// merged MbdChargeHistMaker output and write the knee table of steps "smap" and "ue".
//   hadd knee_all.root knee_*.root
//   root -l -b -q 'make_knee_table.C("knee_all.root", "knee.txt")'
// Runs whose fit fails get knee -1 and are skipped by the later steps (ABORTRUN).

#include <rhouecalib/RhoUECalibPayload.h>

R__LOAD_LIBRARY(librhouecalib.so)

void make_knee_table(const std::string &sums = "knee_all.root", const std::string &table = "knee.txt")
{
  RhoUECalibPayload::FitMbdKnees(sums, table);
}
