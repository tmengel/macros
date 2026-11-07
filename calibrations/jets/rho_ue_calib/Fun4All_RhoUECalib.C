#ifndef FUN4ALL_RHOUECALIB_C
#define FUN4ALL_RHOUECALIB_C

// Rho UE calibration from DST_CALOFITTING (+ DST_ZDC_RAW of the same run and segment for
// the minimum-bias classifier). The input is
// one DST_CALOFITTING file or a ".list" of them (segments of one run: the CDB timestamp is the
// run's). Three steps, run in order over the whole dataset; the output of each job is merged
// with hadd and turned into the input of the next step with the make_*.C macros (see
// README.md; batch scripts in condor/):
//   "knee"  MbdChargeHistMaker: MBD charge and t0 per run        -> make_knee_table.C
//   "smap"  TowerShapeMapMaker: tower shape map sums             -> make_tower_shape_payload.C
//   "ue"    DetermineTowerRho x 3 (with the s-map if given) and
//           RhoUECalibMaker: w / alpha / beta sums               -> make_rho_ue_payload.C
// Events: Au+Au minimum bias with an MBD vertex inside the class range in z (the makers'
// selection; the bunch-crossing and MBD charge selection is the MBD reconstruction's).
//
//   root -l -b -q 'Fun4All_RhoUECalib.C(0, "DST_CALOFITTING_run2auau_pro001_pcdb001_v001-00054912-00000.root", "knee_54912_0.root", "knee")'
//   root -l -b -q 'Fun4All_RhoUECalib.C(0, "<file or list>", "smap_54912_0.root", "smap", "newcdbtag", "knee.txt")'
//   root -l -b -q 'Fun4All_RhoUECalib.C(0, "<file or list>", "ue_54912_0.root", "ue", "newcdbtag", "knee.txt", "smap_payload.root")'

#include <GlobalVariables.C>

#include <Calo_Calib.C>

#include <caloreco/CaloTowerBuilder.h>
#include <caloreco/CaloTowerCalib.h>
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
#include <rhouecalib/RhoUECalibBase.h>
#include <rhouecalib/RhoUECalibMaker.h>
#include <rhouecalib/TowerShapeMapMaker.h>

#include <zdcinfo/ZdcReco.h>

#include <TSystem.h>

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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
    // DST_CALOFITTING file, or a ".list" of DST_CALOFITTING files of one run
    const std::string &infile_calo = "DST_CALOFITTING_run2auau_pro001_pcdb001_v001-00054912-00000.root",
    const std::string &outfile = "rho_ue_calib.root",
    // "knee", "smap" or "ue"
    const std::string &step = "knee",
    const std::string &cdbtag = "newcdbtag",
    // knee table of make_knee_table.C ("smap" and "ue"): Q_knee and main-bunch t0 per run
    const std::string &knee_table = "",
    // tower shape map payload of make_tower_shape_payload.C ("ue"; "" = rho without the
    // s-map: the UE calibration must be made with the rho configuration it is applied with)
    const std::string &tower_shape_payload = "",
    // seed jet acceptance of the per-layer rho ("ue"): < 0 = the tower eta range shrunk by R
    // (the DetermineTowerRho / HIJetReco.C default), else |eta_jet| < seed_abs_eta (5 = every jet)
    const float seed_abs_eta = -1,
    // ZS cross calibration of the towers ("smap", "ue"): "" = the run's own CDB payload (abort if
    // missing), "off" = none, else a list "run payload_run CEMC_url HCALIN_url HCALOUT_url" with
    // the nearest run's payloads for runs without their own (make_zs_crosscalib_list.C)
    const std::string &zs_calib_list = "")
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

  std::vector<std::string> calo_files;
  if (infile_calo.size() > 5 && infile_calo.compare(infile_calo.size() - 5, 5, ".list") == 0)
  {
    std::ifstream list(infile_calo);
    std::string line;
    while (list >> line)
    {
      calo_files.push_back(line);
    }
  }
  else
  {
    calo_files.push_back(infile_calo);
  }
  if (calo_files.empty())
  {
    std::cout << "Fun4All_RhoUECalib: no input files in " << infile_calo << std::endl;
    gSystem->Exit(1);
  }
  const auto runseg = Fun4AllUtils::GetRunSegment(calo_files.front());
  for (const auto &file : calo_files)
  {
    if (file.find("CALOFITTING") == std::string::npos || Fun4AllUtils::GetRunSegment(file).first != runseg.first)
    {
      std::cout << "Fun4All_RhoUECalib: " << file << " is not a DST_CALOFITTING file of run " << runseg.first << std::endl;
      gSystem->Exit(1);
    }
  }

  Enable::VERBOSITY = 0;

  auto *se = Fun4AllServer::instance();
  se->Verbosity(0);
  auto *rc = recoConsts::instance();
  rc->set_StringFlag("CDB_GLOBALTAG", cdbtag);
  rc->set_uint64Flag("TIMESTAMP", runseg.first);
  CDBInterface::instance()->Verbosity(0);
  se->registerSubsystem(new FlagHandler());

  // DST_CALOFITTING and the ZDC raw DST of the same run and segments
  auto *incalo = new Fun4AllDstInputManager("DSTINPUT_CALO");
  auto *inzdc = new Fun4AllDstInputManager("DSTINPUT_ZDC");
  for (const auto &file : calo_files)
  {
    std::string file_zdc = file;
    file_zdc.replace(file_zdc.find("CALOFITTING"), std::string("CALOFITTING").size(), "ZDC_RAW");
    incalo->AddFile(file);
    inzdc->AddFile(file_zdc);
  }
  se->registerInputManager(incalo);
  se->registerInputManager(inzdc);
  auto *ingeom = new Fun4AllRunNodeInputManager("DST_GEO");
  ingeom->AddFile(CDBInterface::instance()->getUrl("calo_geo"));
  se->registerInputManager(ingeom);

  // MBD, ZDC, vertex, minimum-bias classifier
  se->registerSubsystem(new TriggerRunInfoReco());
  se->registerSubsystem(new MbdReco());
  auto *caZDC = new CaloTowerBuilder("ZDCBUILDER");
  caZDC->set_detector_type(CaloTowerDefs::ZDC);
  caZDC->set_builder_type(CaloTowerDefs::kPRDFTowerv4);
  caZDC->set_processing_type(CaloWaveformProcessing::FAST);
  se->registerSubsystem(caZDC);
  auto *zdc = new ZdcReco();
  zdc->set_zdc1_cut(0.0);
  zdc->set_zdc2_cut(0.0);
  se->registerSubsystem(zdc);
  se->registerSubsystem(new GlobalVertexReco());
  se->registerSubsystem(new MinimumBiasClassifier());

  // calorimeters ("smap", "ue"; the knee needs only the MBD): calibrated towers and the
  // EMCal retowered onto the HCal grid (TOWERINFO_CALIB_CEMC_RETOWER)
  if (step != "knee")
  {
    Process_Calo_Calib();
    // ZS cross calibration: the run's own payload, none, or the list's (nearest run)
    std::map<std::string, std::string> zs_url;
    if (!zs_calib_list.empty() && zs_calib_list != "off")
    {
      std::ifstream zsin(zs_calib_list);
      std::string line;
      while (std::getline(zsin, line))
      {
        std::istringstream ss(line);
        int zrun = 0;
        int zsrc = 0;
        std::string ucemc;
        std::string uihcal;
        std::string uohcal;
        if ((ss >> zrun >> zsrc >> ucemc >> uihcal >> uohcal) && zrun == runseg.first)
        {
          zs_url = {{"CEMCCALIB", ucemc}, {"HCALIN", uihcal}, {"HCALOUT", uohcal}};
          std::cout << "Fun4All_RhoUECalib: ZS cross calibration of run " << zsrc << std::endl;
        }
      }
      if (zs_url.empty())
      {
        std::cout << "Fun4All_RhoUECalib: run " << runseg.first << " not in " << zs_calib_list << std::endl;
        gSystem->Exit(1);
      }
    }
    for (const char *calibnode : {"CEMCCALIB", "HCALOUT", "HCALIN"})
    {
      auto *calib = dynamic_cast<CaloTowerCalib *>(se->getSubsysReco(calibnode));
      if (!calib)
      {
        continue;
      }
      if (zs_calib_list == "off")
      {
        calib->set_doZScrosscalib(false);
        calib->set_doAbortNoZSCalib(false);
        continue;
      }
      calib->set_doZScrosscalib(true);
      calib->set_doAbortNoZSCalib(true);
      if (!zs_url.empty())
      {
        calib->set_directURL_ZScrosscalib(zs_url[calibnode]);
      }
    }
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
    // rho per layer exactly as in HIJetReco.C (MULT, 2 seeds omitted)
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
