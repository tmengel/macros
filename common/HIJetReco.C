#ifndef MACRO_HIJETRECO_C
#define MACRO_HIJETRECO_C

#include <GlobalVariables.C>

#include <g4jets/TruthJetInput.h>

#include <globalvertex/GlobalVertex.h>

#include <jetbackground/CopyAndSubtractJets.h>
#include <jetbackground/DetermineEventRho.h>
#include <jetbackground/DetermineTowerBackground.h>
#include <jetbackground/DetermineTowerRho.h>
#include <jetbackground/FastJetAlgoSub.h>
#include <jetbackground/RetowerCEMC.h>
#include <jetbackground/SubtractTowers.h>
#include <jetbackground/SubtractTowersCS.h>
#include <jetbackground/TowerRho.h>
#include <jetbackground/TowerRhoToBackground.h>
#include <jetbackground/TowerRhov1.h>

#include <jetbase/FastJetOptions.h>
#include <jetbase/JetReco.h>
#include <jetbase/TowerJetInput.h>
#include <jetbase/TrackJetInput.h>

#include <particleflowreco/ParticleFlowJetInput.h>

#include <eventplaneinfo/EventPlaneReco.h>
#include <eventplaneinfo/Eventplaneinfo.h>

#include <fun4all/Fun4AllServer.h>

R__LOAD_LIBRARY(libg4jets.so)
R__LOAD_LIBRARY(libglobalvertex.so)
R__LOAD_LIBRARY(libjetbackground.so)
R__LOAD_LIBRARY(libjetbase.so)
R__LOAD_LIBRARY(libparticleflow.so)
R__LOAD_LIBRARY(libeventplaneinfo.so)

// ----------------------------------------------------------------------------
//! General options for background-subtracted jet reconstruction
// ----------------------------------------------------------------------------
namespace Enable
{
  bool HIJETS = false;        ///< do HI jet reconstruction
  int HIJETS_VERBOSITY = 0;   ///< verbosity
  bool HIJETS_MC = false;     ///< is simulation
  bool HIJETS_TRUTH = false;  ///< make truth jets
  bool HIJETS_TOWER = true;   ///< make tower jets
  bool HIJETS_TRACK = false;  ///< make track jets
  bool HIJETS_PFLOW = false;  ///< make particle flow jets
}  // namespace Enable

// ----------------------------------------------------------------------------
//! Options specific to background-subtracted jet reconstruction
// ----------------------------------------------------------------------------
namespace HIJETS
{
  // do_flow = 0 --noflow
  // do_flow = 1 --psi2 derived from calo
  // do_flow = 2 --psi2 derived from HIJING
  // do_flow = 3 --psi2 derived from sEPD
  int do_flow = 0;

  ///! do constituent subtraction
  bool do_CS = false;

  ///! turn on/off functionality only relevant for
  ///! nucleon collisions
  bool is_pp = true;

  ///! sets prefix of nodes to use as tower jet
  ///! input
  std::string tower_prefix = "TOWERINFO_CALIB";

  ///! turn on/off vertex specification
  bool do_vertex_type = true;

  ///! if specifying vertex, set vertex
  ///! to use to this one
  GlobalVertex::VTXTYPE vertex_type = GlobalVertex::MBD;

  ///! Base fastjet options to use. Note that the
  ///! resolution parameter will be overwritten
  ///! to R = 0.2, 0.3, 0.4, and 0.5
  FastJetOptions fj_opts({Jet::ANTIKT, JET_R, 0.4, VERBOSITY, static_cast<float>(Enable::HIJETS_VERBOSITY)});

  ///! sets jet node name
  std::string jet_node = "ANTIKT";

  ///! sets prefix of nodes to store jets
  std::string algo_prefix = "AntiKt";

  ///! also make jets from rho-subtracted towers (requires Enable::HIJETS_TOWER):
  ///!   DetermineTowerRho (one per layer) -> TowerRhoToBackground -> SubtractTowers
  ///! The subtracted towers are written to <rho_sub_prefix>_CEMC_RETOWER_SUB1,
  ///! <rho_sub_prefix>_HCALIN_SUB1 and <rho_sub_prefix>_HCALOUT_SUB1, and the jets
  ///! to <algo_prefix>_Tower_r0X_RhoSub
  bool do_rho_sub = false;

  ///! rho method used for the subtraction (TowerRho::Method::MULT or AREA)
  TowerRho::Method rho_sub_method = TowerRho::Method::MULT;

  ///! prefix of the rho-subtracted tower nodes
  std::string rho_sub_prefix = "TOWERINFO_RHOSUB";

  ///! optional eta-shape calibration of the rho UE: a CDB tag, or a direct path to
  ///! the calibration file (used until it is in the CDB; wins if both are set).
  ///! Both empty = no calibration.
  std::string rho_sub_eta_calib_tag = "";
  std::string rho_sub_eta_calib_path = "";

  ///! UE model of that calibration: affine UE = alpha x + beta (default) or w x
  bool rho_sub_affine = true;

  ///! optional tower shape map (s-map) for the seed jets of the per-layer rho
  ///! ("seed only"; the eta-shape calibration must be made with the same setting):
  ///! a CDB tag, or a direct path (wins if both are set). Both empty = off.
  std::string rho_sub_tower_shape_tag = "";
  std::string rho_sub_tower_shape_path = "";

  ///! sets the embedding g4 flag used to determine which particles are classified as 'truth' in MakeHITruthJets
  ///! if you don't know what this is, you probably don't need to change it
  ///! 0 = all particles (use for all g4particles !not useful for HIJING+Pythia samples)
  ///! 1 = pythia/herwig particles only (use for pythia/herwig jets in HIJING+Pythia samples)
  ///! 2 = pythia particles from the HIJING+Pythia samples (use for pythia jets in HIJING+Pythia samples)
  ///! negative values are typically background particles (HIJING particles)
  int embedding_flag = 1;

  ///! enumerates reconstructed resolution
  ///! parameters
  enum Res
  {
    R02 = 0,
    R03 = 1,
    R04 = 2,
    R05 = 3
  };

  // --------------------------------------------------------------------------
  //! Helper method to generate releveant FastJet algorithms
  // --------------------------------------------------------------------------
  FastJetAlgoSub *GetFJAlgo(const float reso)
  {
    // grab current options & update
    // reso parameter
    FastJetOptions opts = fj_opts;
    opts.jet_R = reso;

    // create new algorithm
    return new FastJetAlgoSub(opts);

  }  // end 'GetFJAlgo()'
}  // namespace HIJETS

// ----------------------------------------------------------------------------
//! Make jets out of appropriate truth particles
// ----------------------------------------------------------------------------
void MakeHITruthJets()
{
  // set verbosity
  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  Fun4AllServer *se = Fun4AllServer::instance();

  // if making track jets, make truth jets out of only charged particles
  if (Enable::HIJETS_TRACK)
  {
    // configure truth jet input for charged particles
    TruthJetInput *ctji = new TruthJetInput(Jet::SRC::CHARGED_PARTICLE);
    ctji->add_embedding_flag(HIJETS::embedding_flag);  // changes depending on signal vs. embedded

    // book jet reconstruction on chargedparticles
    JetReco *chargedtruthjetreco = new JetReco();
    chargedtruthjetreco->add_input(ctji);
    chargedtruthjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_ChargedTruth_r02");
    chargedtruthjetreco->add_algo(HIJETS::GetFJAlgo(0.3), HIJETS::algo_prefix + "_ChargedTruth_r03");
    chargedtruthjetreco->add_algo(HIJETS::GetFJAlgo(0.4), HIJETS::algo_prefix + "_ChargedTruth_r04");
    chargedtruthjetreco->add_algo(HIJETS::GetFJAlgo(0.5), HIJETS::algo_prefix + "_ChargedTruth_r05");
    chargedtruthjetreco->set_algo_node(HIJETS::jet_node);
    chargedtruthjetreco->set_input_node("TRUTH");
    chargedtruthjetreco->Verbosity(verbosity);
    se->registerSubsystem(chargedtruthjetreco);
  }

  // if making tower or pflow jets, make truth jets out of all particles
  if (Enable::HIJETS_TOWER || Enable::HIJETS_PFLOW)
  {
    // configure truth jet input for all particles
    TruthJetInput *tji = new TruthJetInput(Jet::PARTICLE);
    tji->add_embedding_flag(HIJETS::embedding_flag);  // changes depending on signal vs. embedded

    // book jet reconstruction on all particles
    JetReco *truthjetreco = new JetReco();
    truthjetreco->add_input(tji);
    truthjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_Truth_r02");
    truthjetreco->add_algo(HIJETS::GetFJAlgo(0.3), HIJETS::algo_prefix + "_Truth_r03");
    truthjetreco->add_algo(HIJETS::GetFJAlgo(0.4), HIJETS::algo_prefix + "_Truth_r04");
    truthjetreco->add_algo(HIJETS::GetFJAlgo(0.5), HIJETS::algo_prefix + "_Truth_r05");
    truthjetreco->set_algo_node(HIJETS::jet_node);
    truthjetreco->set_input_node("TRUTH");
    truthjetreco->Verbosity(verbosity);
    se->registerSubsystem(truthjetreco);
  }

  // exit back to HIJetReco()
  return;
}

// ----------------------------------------------------------------------------
//! Make jets out of subtracted towers
// ----------------------------------------------------------------------------
void MakeHITowerJets()
{
  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  Fun4AllServer *se = Fun4AllServer::instance();

  if (HIJETS::do_flow == 3)
  {
    EventPlaneReco *epreco = new EventPlaneReco();
    se->registerSubsystem(epreco);
  }

  RetowerCEMC *rcemc = new RetowerCEMC();
  rcemc->Verbosity(verbosity);
  rcemc->set_towerinfo(true);
  rcemc->set_frac_cut(1);  // fraction of retower that must be masked to mask the full retower. 1=do not apply extra masking
  rcemc->set_do_rescale(false); // scale the retowered towers up to account for dead area? 
  rcemc->set_towerNodePrefix(HIJETS::tower_prefix);
  se->registerSubsystem(rcemc);

  JetReco *towerjetreco = new JetReco();
  TowerJetInput *incemc = new TowerJetInput(Jet::CEMC_TOWERINFO_RETOWER, HIJETS::tower_prefix);
  TowerJetInput *inihcal = new TowerJetInput(Jet::HCALIN_TOWERINFO, HIJETS::tower_prefix);
  TowerJetInput *inohcal = new TowerJetInput(Jet::HCALOUT_TOWERINFO, HIJETS::tower_prefix);
  if (HIJETS::do_vertex_type)
  {
    incemc->set_GlobalVertexType(HIJETS::vertex_type);
    inihcal->set_GlobalVertexType(HIJETS::vertex_type);
    inohcal->set_GlobalVertexType(HIJETS::vertex_type);
  }
  towerjetreco->add_input(incemc);
  towerjetreco->add_input(inihcal);
  towerjetreco->add_input(inohcal);
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_TowerInfo_HIRecoSeedsRaw_r02");
  towerjetreco->set_algo_node(HIJETS::jet_node);
  towerjetreco->set_input_node("TOWER");
  towerjetreco->Verbosity(verbosity);
  se->registerSubsystem(towerjetreco);

  DetermineTowerBackground *dtb = new DetermineTowerBackground();
  dtb->SetBackgroundOutputName("TowerInfoBackground_Sub1");
  dtb->SetFlow(HIJETS::do_flow);
  dtb->SetSeedType(0);
  dtb->SetSeedJetD(3);
  // dtb->set_towerinfo(true);
  dtb->Verbosity(verbosity);
  dtb->set_towerNodePrefix(HIJETS::tower_prefix);
  se->registerSubsystem(dtb);

  CopyAndSubtractJets *casj = new CopyAndSubtractJets();
  casj->SetFlowModulation(HIJETS::do_flow);
  casj->Verbosity(verbosity);
  casj->set_towerinfo(true);
  casj->set_towerNodePrefix(HIJETS::tower_prefix);
  se->registerSubsystem(casj);

  DetermineTowerBackground *dtb2 = new DetermineTowerBackground();
  dtb2->SetBackgroundOutputName("TowerInfoBackground_Sub2");
  dtb2->SetFlow(HIJETS::do_flow);
  dtb2->SetSeedType(1);
  dtb2->SetSeedJetPt(7);
  dtb2->Verbosity(verbosity);
  // dtb2->set_towerinfo(true);
  dtb2->set_towerNodePrefix(HIJETS::tower_prefix);
  se->registerSubsystem(dtb2);

  SubtractTowers *st = new SubtractTowers();
  st->SetFlowModulation(HIJETS::do_flow);
  st->Verbosity(verbosity);
  st->set_towerinfo(true);
  st->set_towerNodePrefix(HIJETS::tower_prefix);
  se->registerSubsystem(st);

  towerjetreco = new JetReco();
  incemc = new TowerJetInput(Jet::CEMC_TOWERINFO_SUB1, HIJETS::tower_prefix);
  inihcal = new TowerJetInput(Jet::HCALIN_TOWERINFO_SUB1, HIJETS::tower_prefix);
  inohcal = new TowerJetInput(Jet::HCALOUT_TOWERINFO_SUB1, HIJETS::tower_prefix);
  if (HIJETS::do_vertex_type)
  {
    incemc->set_GlobalVertexType(HIJETS::vertex_type);
    inihcal->set_GlobalVertexType(HIJETS::vertex_type);
    inohcal->set_GlobalVertexType(HIJETS::vertex_type);
  }
  towerjetreco->add_input(incemc);
  towerjetreco->add_input(inihcal);
  towerjetreco->add_input(inohcal);
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_Tower_r02_Sub1");
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.3), HIJETS::algo_prefix + "_Tower_r03_Sub1");
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.4), HIJETS::algo_prefix + "_Tower_r04_Sub1");
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.5), HIJETS::algo_prefix + "_Tower_r05_Sub1");
  towerjetreco->set_algo_node(HIJETS::jet_node);
  towerjetreco->set_input_node("TOWER");
  towerjetreco->Verbosity(verbosity);
  se->registerSubsystem(towerjetreco);

  return;
}

// ----------------------------------------------------------------------------
//! Make jets out of rho-subtracted towers. Must run after MakeHITowerJets(),
//! which registers RetowerCEMC.
// ----------------------------------------------------------------------------
void MakeHIRhoSubTowerJets()
{
  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  Fun4AllServer *se = Fun4AllServer::instance();

  // rho per layer, e.g. TowerRho_CEMC_MULT (seed jets in TowerRho_CEMC_MULT_SeedJets)
  const std::string method = TowerRhov1::get_method_string(HIJETS::rho_sub_method);
  const std::vector<std::pair<Jet::SRC, std::string>> layers = {
      {Jet::CEMC_TOWERINFO_RETOWER, "CEMC"},
      {Jet::HCALIN_TOWERINFO, "HCALIN"},
      {Jet::HCALOUT_TOWERINFO, "HCALOUT"}};
  std::vector<std::string> rho_nodes;
  for (const auto &[src, layer] : layers)
  {
    rho_nodes.push_back("TowerRho_" + layer + "_" + method);
    DetermineTowerRho *rhoCalc = new DetermineTowerRho("DetermineTowerRho_" + layer);
    rhoCalc->add_method(HIJETS::rho_sub_method, rho_nodes.back());
    TowerJetInput *rho_in = new TowerJetInput(src, HIJETS::tower_prefix);
    if (HIJETS::do_vertex_type)
    {
      rho_in->set_GlobalVertexType(HIJETS::vertex_type);
    }
    rhoCalc->add_input(rho_in);
    if (!HIJETS::rho_sub_tower_shape_tag.empty())
    {
      rhoCalc->set_tower_shape_cdb_tag(HIJETS::rho_sub_tower_shape_tag);
    }
    if (!HIJETS::rho_sub_tower_shape_path.empty())
    {
      rhoCalc->set_tower_shape_path(HIJETS::rho_sub_tower_shape_path);
    }
    if (HIJETS::do_vertex_type)
    {
      rhoCalc->set_vertex_type(HIJETS::vertex_type);
    }
    rhoCalc->Verbosity(verbosity);
    se->registerSubsystem(rhoCalc);
  }

  // turn the three rho values into a TowerBackground
  TowerRhoToBackground *rho2bkg = new TowerRhoToBackground();
  rho2bkg->set_emcal_rho_node(rho_nodes[0]);
  rho2bkg->set_ihcal_rho_node(rho_nodes[1]);
  rho2bkg->set_ohcal_rho_node(rho_nodes[2]);
  rho2bkg->set_background_node("TowerInfoBackground_RhoSub");
  if (HIJETS::do_vertex_type)
  {
    rho2bkg->set_vertex_type(HIJETS::vertex_type);
  }
  if (!HIJETS::rho_sub_eta_calib_tag.empty())
  {
    rho2bkg->set_eta_calib_cdb_tag(HIJETS::rho_sub_eta_calib_tag);
  }
  if (!HIJETS::rho_sub_eta_calib_path.empty())
  {
    rho2bkg->set_eta_calib_path(HIJETS::rho_sub_eta_calib_path);
  }
  rho2bkg->set_calib_model(HIJETS::rho_sub_affine ? TowerRhoToBackground::CalibModel::AFFINE : TowerRhoToBackground::CalibModel::MULTIPLICATIVE);
  rho2bkg->Verbosity(verbosity);
  se->registerSubsystem(rho2bkg);

  // subtract it, writing to separate nodes so the HI _SUB1 towers are untouched
  SubtractTowers *st = new SubtractTowers("SubtractTowers_RhoSub");
  st->set_towerNodePrefix(HIJETS::tower_prefix);
  st->set_inputTowerBackgroundNode("TowerInfoBackground_RhoSub");
  st->set_emcal_output_node(HIJETS::rho_sub_prefix + "_CEMC_RETOWER_SUB1");
  st->set_ihcal_output_node(HIJETS::rho_sub_prefix + "_HCALIN_SUB1");
  st->set_ohcal_output_node(HIJETS::rho_sub_prefix + "_HCALOUT_SUB1");
  st->Verbosity(verbosity);
  se->registerSubsystem(st);

  JetReco *towerjetreco = new JetReco("TowerJetReco_RhoSub");
  TowerJetInput *incemc = new TowerJetInput(Jet::CEMC_TOWERINFO_SUB1, HIJETS::rho_sub_prefix);
  TowerJetInput *inihcal = new TowerJetInput(Jet::HCALIN_TOWERINFO_SUB1, HIJETS::rho_sub_prefix);
  TowerJetInput *inohcal = new TowerJetInput(Jet::HCALOUT_TOWERINFO_SUB1, HIJETS::rho_sub_prefix);
  if (HIJETS::do_vertex_type)
  {
    incemc->set_GlobalVertexType(HIJETS::vertex_type);
    inihcal->set_GlobalVertexType(HIJETS::vertex_type);
    inohcal->set_GlobalVertexType(HIJETS::vertex_type);
  }
  towerjetreco->add_input(incemc);
  towerjetreco->add_input(inihcal);
  towerjetreco->add_input(inohcal);
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_Tower_r02_RhoSub");
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.3), HIJETS::algo_prefix + "_Tower_r03_RhoSub");
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.4), HIJETS::algo_prefix + "_Tower_r04_RhoSub");
  towerjetreco->add_algo(HIJETS::GetFJAlgo(0.5), HIJETS::algo_prefix + "_Tower_r05_RhoSub");
  towerjetreco->set_algo_node(HIJETS::jet_node);
  towerjetreco->set_input_node("TOWER");
  towerjetreco->Verbosity(verbosity);
  se->registerSubsystem(towerjetreco);

  return;
}

// ----------------------------------------------------------------------------
//! Make jets out of tracks with background subtraction
// ----------------------------------------------------------------------------
void MakeHITrackJets()
{
  // set verbosity
  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  Fun4AllServer *se = Fun4AllServer::instance();

  // emit warning: background sub will be added later
  std::cerr << "WARNING: Background subtraction for track jets is still in development!\n"
            << "  If you want to do jet reco without background subtraction, please\n"
            << "  use NoBkgdSubJetReco()"
            << std::endl;

  // book jet reconstruction routines on tracks
  JetReco *trackjetreco = new JetReco();
  trackjetreco->add_input(new TrackJetInput(Jet::SRC::TRACK));
  trackjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_Track_r02");
  trackjetreco->add_algo(HIJETS::GetFJAlgo(0.3), HIJETS::algo_prefix + "_Track_r03");
  trackjetreco->add_algo(HIJETS::GetFJAlgo(0.4), HIJETS::algo_prefix + "_Track_r04");
  trackjetreco->add_algo(HIJETS::GetFJAlgo(0.5), HIJETS::algo_prefix + "_Track_r05");
  trackjetreco->set_algo_node(HIJETS::jet_node);
  trackjetreco->set_input_node("TRACK");
  trackjetreco->Verbosity(verbosity);
  se->registerSubsystem(trackjetreco);

  // exit back to HIJetReco()
  return;
}

// ----------------------------------------------------------------------------
//! Make jets out of particle-flow elements with background subtraction
// ----------------------------------------------------------------------------
void MakeHIPFlowJets()
{
  // set verbosity
  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  Fun4AllServer *se = Fun4AllServer::instance();

  // emit warning: background sub will be added later
  std::cerr << "WARNING: Background subtraction for particle-flow jets is still in development!\n"
            << "  If you want to do jet reco without background subtraction, please\n"
            << "  use NoBkgdSubJetReco.C"
            << std::endl;

  // book jet reconstruction routines on pflow elements
  JetReco *pflowjetreco = new JetReco();
  pflowjetreco->add_input(new ParticleFlowJetInput());
  pflowjetreco->add_algo(HIJETS::GetFJAlgo(0.2), HIJETS::algo_prefix + "_ParticleFlow_r02");
  pflowjetreco->add_algo(HIJETS::GetFJAlgo(0.3), HIJETS::algo_prefix + "_ParticleFlow_r03");
  pflowjetreco->add_algo(HIJETS::GetFJAlgo(0.4), HIJETS::algo_prefix + "_ParticleFlow_r04");
  pflowjetreco->add_algo(HIJETS::GetFJAlgo(0.5), HIJETS::algo_prefix + "_ParticleFlow_r05");
  pflowjetreco->set_algo_node(HIJETS::jet_node);
  pflowjetreco->set_input_node("ELEMENT");
  pflowjetreco->Verbosity(verbosity);
  se->registerSubsystem(pflowjetreco);

  // exit back to HIJetReco()
  return;
}

// ----------------------------------------------------------------------------
//! Run background-subtracted jet reconstruction
// ----------------------------------------------------------------------------
void HIJetReco()
{
  // if simulation, make appropriate truth jets
  if (Enable::HIJETS_MC && Enable::HIJETS_TRUTH) MakeHITruthJets();

  // run approriate jet reconstruction routines
  if (Enable::HIJETS_TOWER) MakeHITowerJets();
  if (Enable::HIJETS_TOWER && HIJETS::do_rho_sub) MakeHIRhoSubTowerJets();
  if (Enable::HIJETS_TRACK) MakeHITrackJets();
  if (Enable::HIJETS_PFLOW) MakeHIPFlowJets();
}

// ----------------------------------------------------------------------------
//! Determine rho from tower input to jet reco (necessary for jet QA)
// ----------------------------------------------------------------------------
void DoRhoCalculation()
{
  // set verbosity
  //  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  Fun4AllServer *se = Fun4AllServer::instance();

  // run rho calculations w/ towers
  if (Enable::HIJETS_TOWER)
  {
    DetermineTowerRho *towRhoCalc = new DetermineTowerRho();
    towRhoCalc->add_method(TowerRho::Method::AREA);
    towRhoCalc->add_method(TowerRho::Method::MULT);
    TowerJetInput *rho_incemc = new TowerJetInput(Jet::CEMC_TOWERINFO_RETOWER, HIJETS::tower_prefix);
    TowerJetInput *rho_inihcal = new TowerJetInput(Jet::HCALIN_TOWERINFO, HIJETS::tower_prefix);
    TowerJetInput *rho_inohcal = new TowerJetInput(Jet::HCALOUT_TOWERINFO, HIJETS::tower_prefix);
    if (HIJETS::do_vertex_type)
    {
      rho_incemc->set_GlobalVertexType(HIJETS::vertex_type);
      rho_inihcal->set_GlobalVertexType(HIJETS::vertex_type);
      rho_inohcal->set_GlobalVertexType(HIJETS::vertex_type);
    }
    towRhoCalc->add_tower_input(rho_incemc);
    towRhoCalc->add_tower_input(rho_inihcal);
    towRhoCalc->add_tower_input(rho_inohcal);
    se->registerSubsystem(towRhoCalc);
  }

  // run rho calculations w/ tracks
  if (Enable::HIJETS_TRACK)
  {
    DetermineEventRho *trkRhoCalc = new DetermineEventRho();
    trkRhoCalc->add_method(EventRho::Method::AREA, "EventRho_AREA");
    trkRhoCalc->add_method(EventRho::Method::MULT, "EventRho_MULT");
    trkRhoCalc->add_input(new TrackJetInput(Jet::SRC::TRACK));
    se->registerSubsystem(trkRhoCalc);
  }

  // exit back to main macro
  return;
}

#endif
