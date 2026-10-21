#ifndef FUN4ALL_RHOUECALIB_C
#define FUN4ALL_RHOUECALIB_C

// Rho UE calibration from DST_CALOFITTING (+ DST_ZDC_RAW of the same run and segment for
// the minimum-bias classifier), one job per segment. Three steps, run in order over the
// whole dataset; the output of each job is merged with hadd and turned into the input of
// the next step with the make_*.C macros (see README.md):
//   "knee"  MbdChargeHistMaker: MBD charge per run              -> make_knee_table.C
//   "smap"  TowerShapeMapMaker: tower shape map sums             -> make_tower_shape_payload.C
//   "ue"    DetermineTowerRho x 3 (with the s-map if given) and
//           RhoUECalibMaker: w / alpha / beta sums               -> make_rho_ue_payload.C
// Events: Au+Au minimum bias with an MBD vertex inside the class range in z.
//
//   root -l -b -q 'Fun4All_RhoUECalib.C(0, "DST_CALOFITTING_run2auau_ana509_2024p022_v001-00054912-00000.root", "knee_54912_0.root", "knee")'
//   root -l -b -q 'Fun4All_RhoUECalib.C(0, "<file>", "smap_54912_0.root", "smap", "newcdbtag", "knee.txt")'
//   root -l -b -q 'Fun4All_RhoUECalib.C(0, "<file>", "ue_54912_0.root", "ue", "newcdbtag", "knee.txt", "smap_payload.root")'

#include <GlobalVariables.C>

#include <Calo_Calib.C>

#include <caloreco/CaloTowerBuilder.h>
#include <caloreco/CaloTowerDefs.h>
#include <caloreco/CaloWaveformProcessing.h>

#include <calotrigger/MinimumBiasClassifier.h>
#include <calotrigger/TriggerRunInfoReco.h>

#include <ffamodules/CDBInterface.h>
#include <ffamodules/FlagHandler.h>

#include <fun4all/Fun4AllDstInputManager.h>
#include <fun4all/Fun4AllRunNodeInputManager.h>
#include <fun4all/Fun4AllServer.h>
#include <fun4all/Fun4AllUtils.h>

#include <globalvertex/GlobalVertex.h>
#include <globalvertex/GlobalVertexReco.h>

#include <jetbackground/DetermineTowerRho.h>
#include <jetbackground/RetowerCEMC.h>
#include <jetbackground/TowerRho.h>

#include <jetbase/TowerJetInput.h>

#include <mbd/MbdReco.h>

#include <phool/recoConsts.h>

#include <rhouecalib/MbdChargeHistMaker.h>
#include <rhouecalib/RhoUECalibMaker.h>
#include <rhouecalib/TowerShapeMapMaker.h>

#include <zdcinfo/ZdcReco.h>

#include <TSystem.h>

#include <iostream>
#include <string>

R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libffamodules.so)
R__LOAD_LIBRARY(libcalo_reco.so)
R__LOAD_LIBRARY(libmbd.so)
R__LOAD_LIBRARY(libzdcinfo.so)
R__LOAD_LIBRARY(libglobalvertex.so)
R__LOAD_LIBRARY(libcalotrigger.so)
R__LOAD_LIBRARY(libjetbase.so)
R__LOAD_LIBRARY(libjetbackground.so)
R__LOAD_LIBRARY(librhouecalib.so)

void Fun4All_RhoUECalib(
    const int nEvents = 0,
    const std::string &infile_calo = "DST_CALOFITTING_run2auau_ana509_2024p022_v001-00054912-00000.root",
    const std::string &outfile = "rho_ue_calib.root",
    // "knee", "smap" or "ue"
    const std::string &step = "knee",
    const std::string &cdbtag = "newcdbtag",
    // knee table of make_knee_table.C ("smap" and "ue")
    const std::string &knee_table = "",
    // tower shape map payload of make_tower_shape_payload.C ("ue"; "" = rho without the
    // s-map: the UE calibration must be made with the rho configuration it is applied with)
    const std::string &tower_shape_payload = "")
{
  if (step != "knee" && step != "smap" && step != "ue")
  {
    std::cout << "Fun4All_RhoUECalib: unknown step " << step << std::endl;
    gSystem->Exit(1);
  }
  if (step != "knee" && knee_table.empty())
  {
    std::cout << "Fun4All_RhoUECalib: step " << step << " needs the knee table" << std::endl;
    gSystem->Exit(1);
  }

  Enable::VERBOSITY = 0;
  const auto runseg = Fun4AllUtils::GetRunSegment(infile_calo);

  auto *se = Fun4AllServer::instance();
  se->Verbosity(0);
  auto *rc = recoConsts::instance();
  rc->set_StringFlag("CDB_GLOBALTAG", cdbtag);
  rc->set_uint64Flag("TIMESTAMP", runseg.first);
  CDBInterface::instance()->Verbosity(0);
  se->registerSubsystem(new FlagHandler());

  // DST_CALOFITTING and the ZDC raw DST of the same run and segment
  std::string infile_zdc = infile_calo;
  infile_zdc.replace(infile_zdc.find("CALOFITTING"), std::string("CALOFITTING").size(), "ZDC_RAW");
  int ifile = 0;
  for (const auto &infile : {infile_calo, infile_zdc})
  {
    auto *in = new Fun4AllDstInputManager("DSTINPUT_" + std::to_string(ifile++));
    in->AddFile(infile);
    se->registerInputManager(in);
  }
  auto *ingeom = new Fun4AllRunNodeInputManager("DST_GEO");
  ingeom->AddFile(CDBInterface::instance()->getUrl("calo_geo"));
  se->registerInputManager(ingeom);

  // MBD, ZDC, vertex, calorimeter calibration, minimum-bias classifier
  se->registerSubsystem(new TriggerRunInfoReco());
  se->registerSubsystem(new MbdReco());
  auto *caZDC = new CaloTowerBuilder("ZDCBUILDER");
  caZDC->set_detector_type(CaloTowerDefs::ZDC);
  caZDC->set_builder_type(CaloTowerDefs::kPRDFTowerv4);
  caZDC->set_processing_type(CaloWaveformProcessing::FAST);
  caZDC->set_nsamples(16);
  caZDC->set_offlineflag();
  se->registerSubsystem(caZDC);
  auto *zdc = new ZdcReco();
  zdc->set_zdc1_cut(0.0);
  zdc->set_zdc2_cut(0.0);
  se->registerSubsystem(zdc);
  se->registerSubsystem(new GlobalVertexReco());
  Process_Calo_Calib();
  se->registerSubsystem(new MinimumBiasClassifier());

  // EMCal retowered onto the HCal grid (TOWERINFO_CALIB_CEMC_RETOWER)
  auto *rcemc = new RetowerCEMC();
  rcemc->set_towerinfo(true);
  rcemc->set_frac_cut(1.0);
  rcemc->set_do_rescale(false);
  rcemc->set_towerNodePrefix("TOWERINFO_CALIB");
  se->registerSubsystem(rcemc);

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
    // rho per layer exactly as in HIJetReco.C (MULT, default jet acceptance, 2 seeds omitted)
    const std::vector<std::pair<Jet::SRC, std::string>> layers = {
        {Jet::CEMC_TOWERINFO_RETOWER, "CEMC"},
        {Jet::HCALIN_TOWERINFO, "HCALIN"},
        {Jet::HCALOUT_TOWERINFO, "HCALOUT"}};
    for (const auto &[src, layer] : layers)
    {
      auto *rho = new DetermineTowerRho("DetermineTowerRho_" + layer);
      rho->add_method(TowerRho::Method::MULT, "TowerRho_" + layer + "_MULT");
      auto *in = new TowerJetInput(src, "TOWERINFO_CALIB");
      in->set_GlobalVertexType(GlobalVertex::MBD);
      rho->add_input(in);
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
