// Rho UE calibration, after step "smap": the tower shape map payload of
// DetermineTowerRho::set_tower_shape_path (CDBHistos) from the merged TowerShapeMapMaker output.
//   hadd smap_all.root smap_*.root
//   root -l -b -q 'make_tower_shape_payload.C("smap_all.root", "smap_payload.root")'
// max_half_rms: a (layer, class) map whose even- and odd-event halves differ by more than this
// (rms over towers of (s_even - s_odd) / s) is noise and is set to s = 1, e.g. the EMCal of the
// most peripheral classes, where the mean tower E_T is ~0. min_entries: towers with fewer good
// entries in a class get s = 1.

#include <rhouecalib/RhoUECalibPayload.h>

R__LOAD_LIBRARY(librhouecalib.so)

void make_tower_shape_payload(const std::string &sums = "smap_all.root", const std::string &payload = "smap_payload.root",
                              const double max_half_rms = 0.25, const double min_entries = 100)
{
  RhoUECalibPayload::MakeTowerShapePayload(sums, payload, max_half_rms, min_entries);
}
