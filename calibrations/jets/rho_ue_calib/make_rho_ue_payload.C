// Rho UE calibration, after step "ue": the payload of TowerRhoToBackground::set_eta_calib_path
// (CDBHistos: w, alpha, beta per layer, eta strip, vertex z and Q') from the merged
// RhoUECalibMaker output.
//   hadd ue_all.root ue_*.root
//   root -l -b -q 'make_rho_ue_payload.C("ue_all.root", "rho_ue_payload.root")'
// min_events: (strip, z, Q') bins with fewer events take the nearest fitted Q' bin at the same
// z, then the nearest z.

#include <rhouecalib/RhoUECalibPayload.h>

R__LOAD_LIBRARY(librhouecalib.so)

void make_rho_ue_payload(const std::string &sums = "ue_all.root", const std::string &payload = "rho_ue_payload.root",
                         const double min_events = 50)
{
  RhoUECalibPayload::MakeRhoUEPayload(sums, payload, min_events);
}
