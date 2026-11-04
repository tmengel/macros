// Rho UE calibration, after step "knee": one per-run MBD charge knee payload (CDBTTree) per run
// of the knee table, for the CDB (DetermineTowerRho / TowerRhoToBackground set_mbd_knee_cdb_tag,
// or set_mbd_knee_path with the directory before the upload):
//   root -l -b -q 'make_knee_payloads.C("knee.txt", "knee_cdb")'
// -> knee_cdb/mbdq_knee_<run>.root and knee_cdb/mbdq_knee_iov.txt (run, status, knee, file).
// Runs with a failed fit, fewer than min_events events or an edge width more than max_sigma_dev
// (relative) off the median get the median knee of the table (status 1).

#include <rhouecalib/RhoUECalibPayload.h>

R__LOAD_LIBRARY(librhouecalib.so)

void make_knee_payloads(const std::string &table = "knee.txt", const std::string &out_dir = "knee_cdb",
                        double min_events = 1e5, double max_sigma_dev = 0.5)
{
  RhoUECalibPayload::MakeKneePayloads(table, out_dir, min_events, max_sigma_dev);
}
