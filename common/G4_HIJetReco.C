#ifndef MACRO_G4HIJETRECO_C
#define MACRO_G4HIJETRECO_C

#include <GlobalVariables.C>

#include <jetbase/FastJetAlgo.h>
#include <jetbase/JetReco.h>
#include <jetbase/TowerJetInput.h>
#include <g4jets/TruthJetInput.h>

#include <jetbackground/CopyAndSubtractJets.h>
#include <jetbackground/DetermineTowerBackground.h>
#include <jetbackground/DetermineTowerRho.h>
#include <jetbackground/FastJetAlgoSub.h>
#include <jetbackground/RetowerCEMC.h>
#include <jetbackground/SubtractTowers.h>
#include <jetbackground/SubtractTowersCS.h>
#include <jetbackground/TowerRho.h>
#include <jetbackground/TowerRhoToBackground.h>
#include <jetbackground/TowerRhov1.h>

#include <eventplaneinfo/Eventplaneinfo.h>
#include <eventplaneinfo/EventPlaneReco.h>

#include <fun4all/Fun4AllServer.h>

R__LOAD_LIBRARY(libjetbase.so)
R__LOAD_LIBRARY(libg4jets.so)
R__LOAD_LIBRARY(libjetbackground.so)
R__LOAD_LIBRARY(libeventplaneinfo.so)

namespace Enable
{
  bool HIJETS = false;
  int HIJETS_VERBOSITY = 0;
}  // namespace Enable

namespace G4HIJETS
{
  // do_flow = 0 --noflow
  // do_flow = 1 --psi2 derived from calo
  // do_flow = 2 --psi2 derived from HIJING
  // do_flow = 3 --psi2 derived from sEPD
  int do_flow = 0;
  bool do_CS = false;

  // also make jets from rho-subtracted towers:
  //   DetermineTowerRho (one per layer) -> TowerRhoToBackground -> SubtractTowers
  // subtracted towers go to <rho_sub_prefix>_*_SUB1, jets to AntiKt_Tower_r0X_RhoSub
  bool do_rho_sub = false;
  TowerRho::Method rho_sub_method = TowerRho::Method::MULT;
  std::string rho_sub_prefix = "TOWERINFO_RHOSUB";
  // optional eta-shape calibration: CDB tag or direct path (path wins); empty = none
  std::string rho_sub_eta_calib_tag = "";
  std::string rho_sub_eta_calib_path = "";
  // UE model of that calibration: affine (default) or multiplicative
  bool rho_sub_affine = true;
  // tower shape map (s-map) for the rho seed jets: a CDB tag or a direct path; both empty = off
  std::string rho_sub_tower_shape_tag = "";
  std::string rho_sub_tower_shape_path = "";
}  // namespace G4HIJETS

void HIJetReco()
{

  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------

  Fun4AllServer *se = Fun4AllServer::instance();

  JetReco *truthjetreco = new JetReco();
  TruthJetInput *tji = new TruthJetInput(Jet::PARTICLE);
  tji->add_embedding_flag(0);  // changes depending on signal vs. embedded
  truthjetreco->add_input(tji);
  truthjetreco->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.2), "AntiKt_Truth_r02");
  truthjetreco->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.3), "AntiKt_Truth_r03");
  truthjetreco->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.4), "AntiKt_Truth_r04");
  truthjetreco->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.5), "AntiKt_Truth_r05");
  truthjetreco->set_algo_node("ANTIKT");
  truthjetreco->set_input_node("TRUTH");
  truthjetreco->Verbosity(verbosity);
  se->registerSubsystem(truthjetreco);

  if(G4HIJETS::do_flow == 3)
  {
      EventPlaneReco *epreco = new EventPlaneReco();
      se->registerSubsystem(epreco);
  }



  RetowerCEMC *rcemc = new RetowerCEMC(); 
  rcemc->Verbosity(verbosity); 
  rcemc->set_towerinfo(true);
  se->registerSubsystem(rcemc); 

  JetReco *towerjetreco = new JetReco();
  towerjetreco->add_input(new TowerJetInput(Jet::CEMC_TOWERINFO_RETOWER));
  towerjetreco->add_input(new TowerJetInput(Jet::HCALIN_TOWERINFO));
  towerjetreco->add_input(new TowerJetInput(Jet::HCALOUT_TOWERINFO));
  towerjetreco->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.2), "AntiKt_TowerInfo_HIRecoSeedsRaw_r02");
  towerjetreco->set_algo_node("ANTIKT");
  towerjetreco->set_input_node("TOWER");
  towerjetreco->Verbosity(verbosity); 
  se->registerSubsystem(towerjetreco);

 DetermineTowerBackground *dtb = new DetermineTowerBackground();
  dtb->SetBackgroundOutputName("TowerInfoBackground_Sub1");
  dtb->SetFlow(G4HIJETS::do_flow);
  dtb->SetSeedType(0);
  dtb->SetSeedJetD(3);
  // dtb->set_towerinfo(true);
  dtb->Verbosity(verbosity); 
  se->registerSubsystem(dtb);


  CopyAndSubtractJets *casj = new CopyAndSubtractJets();
  casj->SetFlowModulation(G4HIJETS::do_flow);
  casj->Verbosity(verbosity); 
  casj->set_towerinfo(true);
  se->registerSubsystem(casj);
  

  DetermineTowerBackground *dtb2 = new DetermineTowerBackground();
  dtb2->SetBackgroundOutputName("TowerInfoBackground_Sub2");
  dtb2->SetFlow(G4HIJETS::do_flow);
  dtb2->SetSeedType(1);
  dtb2->SetSeedJetPt(7);
  dtb2->Verbosity(verbosity); 
  // dtb2->set_towerinfo(true);
  se->registerSubsystem(dtb2);
  

  SubtractTowers *st = new SubtractTowers();
  st->SetFlowModulation(G4HIJETS::do_flow);
  st->Verbosity(verbosity);
  st->set_towerinfo(true);
  se->registerSubsystem(st);

  towerjetreco = new JetReco();
  towerjetreco->add_input(new TowerJetInput(Jet::CEMC_TOWERINFO_SUB1));
  towerjetreco->add_input(new TowerJetInput(Jet::HCALIN_TOWERINFO_SUB1));
  towerjetreco->add_input(new TowerJetInput(Jet::HCALOUT_TOWERINFO_SUB1));
  towerjetreco->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.2, verbosity), "AntiKt_Tower_r02_Sub1");
  towerjetreco->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.3, verbosity), "AntiKt_Tower_r03_Sub1");
  towerjetreco->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.4, verbosity), "AntiKt_Tower_r04_Sub1");
  towerjetreco->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.5, verbosity), "AntiKt_Tower_r05_Sub1");
  towerjetreco->set_algo_node("ANTIKT");
  towerjetreco->set_input_node("TOWER");
  towerjetreco->Verbosity(verbosity);
  se->registerSubsystem(towerjetreco);

  if (G4HIJETS::do_rho_sub)
  {
    const std::string method = TowerRhov1::get_method_string(G4HIJETS::rho_sub_method);
    const std::vector<std::pair<Jet::SRC, std::string>> layers = {
        {Jet::CEMC_TOWERINFO_RETOWER, "CEMC"},
        {Jet::HCALIN_TOWERINFO, "HCALIN"},
        {Jet::HCALOUT_TOWERINFO, "HCALOUT"}};
    std::vector<std::string> rho_nodes;
    for (const auto &[src, layer] : layers)
    {
      rho_nodes.push_back("TowerRho_" + layer + "_" + method);
      DetermineTowerRho *rhoCalc = new DetermineTowerRho("DetermineTowerRho_" + layer);
      rhoCalc->add_method(G4HIJETS::rho_sub_method, rho_nodes.back());
      rhoCalc->add_input(new TowerJetInput(src));
      if (!G4HIJETS::rho_sub_tower_shape_tag.empty())
      {
        rhoCalc->set_tower_shape_cdb_tag(G4HIJETS::rho_sub_tower_shape_tag);
      }
      if (!G4HIJETS::rho_sub_tower_shape_path.empty())
      {
        rhoCalc->set_tower_shape_path(G4HIJETS::rho_sub_tower_shape_path);
      }
      rhoCalc->Verbosity(verbosity);
      se->registerSubsystem(rhoCalc);
    }

    TowerRhoToBackground *rho2bkg = new TowerRhoToBackground();
    rho2bkg->set_emcal_rho_node(rho_nodes[0]);
    rho2bkg->set_ihcal_rho_node(rho_nodes[1]);
    rho2bkg->set_ohcal_rho_node(rho_nodes[2]);
    rho2bkg->set_background_node("TowerInfoBackground_RhoSub");
    if (!G4HIJETS::rho_sub_eta_calib_tag.empty())
    {
      rho2bkg->set_eta_calib_cdb_tag(G4HIJETS::rho_sub_eta_calib_tag);
    }
    if (!G4HIJETS::rho_sub_eta_calib_path.empty())
    {
      rho2bkg->set_eta_calib_path(G4HIJETS::rho_sub_eta_calib_path);
    }
    rho2bkg->set_calib_model(G4HIJETS::rho_sub_affine ? TowerRhoToBackground::CalibModel::AFFINE : TowerRhoToBackground::CalibModel::MULTIPLICATIVE);
    rho2bkg->Verbosity(verbosity);
    se->registerSubsystem(rho2bkg);

    SubtractTowers *strho = new SubtractTowers("SubtractTowers_RhoSub");
    strho->set_inputTowerBackgroundNode("TowerInfoBackground_RhoSub");
    strho->set_emcal_output_node(G4HIJETS::rho_sub_prefix + "_CEMC_RETOWER_SUB1");
    strho->set_ihcal_output_node(G4HIJETS::rho_sub_prefix + "_HCALIN_SUB1");
    strho->set_ohcal_output_node(G4HIJETS::rho_sub_prefix + "_HCALOUT_SUB1");
    strho->Verbosity(verbosity);
    se->registerSubsystem(strho);

    JetReco *towerjetrecoRho = new JetReco("TowerJetReco_RhoSub");
    towerjetrecoRho->add_input(new TowerJetInput(Jet::CEMC_TOWERINFO_SUB1, G4HIJETS::rho_sub_prefix));
    towerjetrecoRho->add_input(new TowerJetInput(Jet::HCALIN_TOWERINFO_SUB1, G4HIJETS::rho_sub_prefix));
    towerjetrecoRho->add_input(new TowerJetInput(Jet::HCALOUT_TOWERINFO_SUB1, G4HIJETS::rho_sub_prefix));
    towerjetrecoRho->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.2, verbosity), "AntiKt_Tower_r02_RhoSub");
    towerjetrecoRho->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.3, verbosity), "AntiKt_Tower_r03_RhoSub");
    towerjetrecoRho->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.4, verbosity), "AntiKt_Tower_r04_RhoSub");
    towerjetrecoRho->add_algo(new FastJetAlgoSub(Jet::ANTIKT, 0.5, verbosity), "AntiKt_Tower_r05_RhoSub");
    towerjetrecoRho->set_algo_node("ANTIKT");
    towerjetrecoRho->set_input_node("TOWER");
    towerjetrecoRho->Verbosity(verbosity);
    se->registerSubsystem(towerjetrecoRho);
  }

  if (G4HIJETS::do_CS)
  {
    SubtractTowersCS *stCS = new SubtractTowersCS();
    stCS->SetFlowModulation(G4HIJETS::do_flow);
    stCS->SetAlpha(1);
    stCS->SetDeltaRmax(0.3);
    stCS->Verbosity(verbosity);
    se->registerSubsystem(stCS);

    JetReco *towerjetrecoCS = new JetReco();
    towerjetrecoCS->add_input(new TowerJetInput(Jet::CEMC_TOWER_SUB1CS));
    towerjetrecoCS->add_input(new TowerJetInput(Jet::HCALIN_TOWER_SUB1CS));
    towerjetrecoCS->add_input(new TowerJetInput(Jet::HCALOUT_TOWER_SUB1CS));
    // note that CS can use the regular FastJetAlgo without extra modifications for negative-E inputs
    towerjetrecoCS->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.2, verbosity), "AntiKt_Tower_r02_Sub1CS");
    towerjetrecoCS->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.3, verbosity), "AntiKt_Tower_r03_Sub1CS");
    towerjetrecoCS->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.4, verbosity), "AntiKt_Tower_r04_Sub1CS");
    towerjetrecoCS->add_algo(new FastJetAlgo(Jet::ANTIKT, 0.5, verbosity), "AntiKt_Tower_r05_Sub1CS");
    towerjetrecoCS->set_algo_node("ANTIKT");
    towerjetrecoCS->set_input_node("TOWER");
    towerjetrecoCS->Verbosity(verbosity);
    se->registerSubsystem(towerjetrecoCS);
  }

  return;
}
#endif
