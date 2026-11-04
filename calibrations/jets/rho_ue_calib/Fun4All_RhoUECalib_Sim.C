#ifndef FUN4ALL_RHOUECALIB_SIM_C
#define FUN4ALL_RHOUECALIB_SIM_C

// Rho UE calibration from minimum-bias HIJING DSTs (towers already calibrated; MbdOut and
// GlobalVertexMap in the DST), the same three steps and outputs as Fun4All_RhoUECalib.C.
// The input is one DST or a ".list" of DSTs. Minimum bias with the simulation centrality
// scales. No bunch-crossing or off-band veto (none in the simulation); the makers select
// Au+Au minimum bias with an MBD vertex inside the class range in z.
//
//   root -l -b -q 'Fun4All_RhoUECalib_Sim.C(0, "DST_UNSCALED_mb_hijing31_pass2-00000.root", "knee_31_0.root", "knee")'
//   root -l -b -q 'Fun4All_RhoUECalib_Sim.C(0, "<file or list>", "smap_31_0.root", "smap", "knee.txt")'
//   root -l -b -q 'Fun4All_RhoUECalib_Sim.C(0, "<file or list>", "ue_31_0.root", "ue", "knee.txt", "smap_payload.root")'

#include <GlobalVariables.C>

#include <calotrigger/MinimumBiasClassifier.h>

#include <ffamodules/CDBInterface.h>
#include <ffamodules/FlagHandler.h>

#include <fun4all/Fun4AllDstInputManager.h>
#include <fun4all/Fun4AllRunNodeInputManager.h>
#include <fun4all/Fun4AllServer.h>

#include <globalvertex/GlobalVertex.h>

#include <jetbackground/DetermineTowerRho.h>
#include <jetbackground/RetowerCEMC.h>
#include <jetbackground/TowerRho.h>

#include <jetbase/TowerJetInput.h>

#include <phool/recoConsts.h>

#include <rhouecalib/MbdChargeHistMaker.h>
#include <rhouecalib/RhoUECalibBase.h>
#include <rhouecalib/RhoUECalibMaker.h>
#include <rhouecalib/TowerShapeMapMaker.h>

#include <TSystem.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libffamodules.so)
R__LOAD_LIBRARY(libglobalvertex.so)
R__LOAD_LIBRARY(libcalotrigger.so)
R__LOAD_LIBRARY(libjetbase.so)
R__LOAD_LIBRARY(libjetbackground.so)
R__LOAD_LIBRARY(librhouecalib.so)

void Fun4All_RhoUECalib_Sim(
    const int nEvents = 0,
    // HIJING DST, or a ".list" of them
    const std::string &infile = "DST_UNSCALED_mb_hijing31_pass2-00000.root",
    const std::string &outfile = "rho_ue_calib_sim.root",
    // "knee", "smap" or "ue"
    const std::string &step = "knee",
    // knee table of make_knee_table.C ("smap" and "ue")
    const std::string &knee_table = "",
    // tower shape map payload of make_tower_shape_payload.C ("ue"; "" = rho without the s-map)
    const std::string &tower_shape_payload = "",
    // seed jet acceptance of the per-layer rho ("ue"): < 0 = the tower eta range shrunk by R,
    // else |eta_jet| < seed_abs_eta (5 = every jet)
    const float seed_abs_eta = -1,
    const std::string &cdbtag = "MDC2",
    const int run_number = 31,
    // simulation centrality scales (MinimumBiasClassifier), as in the Run 2024 Au+Au HIJING production
    const std::string &centrality_dir = "/sphenix/user/dlis/Projects/centrality/cdb/calibrations")
{
  if (step != "knee" && step != "smap" && step != "ue")
  {
    std::cout << "Fun4All_RhoUECalib_Sim: unknown step " << step << std::endl;
    gSystem->Exit(1);
  }
  if (step != "knee" && knee_table.empty())
  {
    std::cout << "Fun4All_RhoUECalib_Sim: step " << step << " needs the knee table" << std::endl;
    gSystem->Exit(1);
  }

  std::vector<std::string> files;
  if (infile.size() > 5 && infile.compare(infile.size() - 5, 5, ".list") == 0)
  {
    std::ifstream list(infile);
    std::string line;
    while (list >> line)
    {
      files.push_back(line);
    }
  }
  else
  {
    files.push_back(infile);
  }
  if (files.empty())
  {
    std::cout << "Fun4All_RhoUECalib_Sim: no input files in " << infile << std::endl;
    gSystem->Exit(1);
  }

  Enable::VERBOSITY = 0;

  auto *se = Fun4AllServer::instance();
  se->Verbosity(0);
  auto *rc = recoConsts::instance();
  rc->set_StringFlag("CDB_GLOBALTAG", cdbtag);
  rc->set_uint64Flag("TIMESTAMP", run_number);
  CDBInterface::instance()->Verbosity(0);
  se->registerSubsystem(new FlagHandler());

  auto *in = new Fun4AllDstInputManager("DSTINPUT");
  for (const auto &file : files)
  {
    in->AddFile(file);
  }
  se->registerInputManager(in);
  auto *ingeom = new Fun4AllRunNodeInputManager("DST_GEO");
  ingeom->AddFile(CDBInterface::instance()->getUrl("calo_geo"));
  se->registerInputManager(ingeom);

  // minimum bias with the simulation scales
  auto *mb = new MinimumBiasClassifier();
  mb->setIsSim(true);
  mb->setOverwriteScale(centrality_dir + "/scales/cdb_centrality_scale_1.root");
  mb->setOverwriteVtx(centrality_dir + "/vertexscales/cdb_centrality_vertex_scale_1.root");
  se->registerSubsystem(mb);

  // EMCal retowered onto the HCal grid (TOWERINFO_CALIB_CEMC_RETOWER)
  if (step != "knee")
  {
    auto *rcemc = new RetowerCEMC();
    rcemc->set_towerinfo(true);
    rcemc->set_frac_cut(1.0);
    rcemc->set_do_rescale(false);
    rcemc->set_towerNodePrefix("TOWERINFO_CALIB");
    se->registerSubsystem(rcemc);
  }

  RhoUECalibBase *maker = nullptr;
  if (step == "knee")
  {
    maker = new MbdChargeHistMaker(outfile);
  }
  else if (step == "smap")
  {
    maker = new TowerShapeMapMaker(outfile);
  }
  else
  {
    // rho per layer exactly as in G4_HIJetReco.C (MULT, 2 seeds omitted)
    const std::vector<std::pair<Jet::SRC, std::string>> layers = {
        {Jet::CEMC_TOWERINFO_RETOWER, "CEMC"},
        {Jet::HCALIN_TOWERINFO, "HCALIN"},
        {Jet::HCALOUT_TOWERINFO, "HCALOUT"}};
    for (const auto &[src, layer] : layers)
    {
      auto *rho = new DetermineTowerRho("DetermineTowerRho_" + layer);
      rho->add_method(TowerRho::Method::MULT, "TowerRho_" + layer + "_MULT");
      auto *tin = new TowerJetInput(src, "TOWERINFO_CALIB");
      tin->set_GlobalVertexType(GlobalVertex::MBD);
      rho->add_input(tin);
      if (seed_abs_eta >= 0)
      {
        rho->set_jet_abs_eta(seed_abs_eta);
      }
      if (!tower_shape_payload.empty())
      {
        rho->set_tower_shape_path(tower_shape_payload);
        rho->set_vertex_type(GlobalVertex::MBD);
      }
      se->registerSubsystem(rho);
    }
    maker = new RhoUECalibMaker(outfile);
  }
  maker->set_vertex_type(GlobalVertex::MBD);
  if (!knee_table.empty())
  {
    maker->set_knee_table(knee_table);
  }
  se->registerSubsystem(maker);

  se->run(nEvents);
  se->End();
  delete se;
  std::cout << "Done4All" << std::endl;
  gSystem->Exit(0);
}

#endif
