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

#include <jetbackground/SubtractTowersRho.h>
#include <jetbackground/DetermineTowerBackgroundv1.h>
#include <jetbackground/DetermineTowerBackgroundv2.h>

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

  bool HIJETS_ENABLE_TEST = false; ///< enable test modules for jet reco (e.g. background estimation closure tests, etc.)
}  // namespace Enable

// ----------------------------------------------------------------------------
//! Options specific to background-subtracted jet reconstruction
// ----------------------------------------------------------------------------
namespace HIJETS
{
   
  
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

  // phi reweight .. should just be true without option to change
  bool do_reweight = true; // if true, use reweighting to account for flow modulation in background estimation, if false, just exclude all towers in eta strips with seeds and/or bad towers

  std::string EPD_tower_node = "TOWERINFO_CALIB_SEPD";

  ///! sets algo of seed jets
  Jet::ALGO seed_algo  = Jet::ANTIKT;
  float seed_R = 0.2;
  int n_omit_seeds = -1;
  float DR = 0.4; // exclusion radius for seed jets
  bool exclude_full_eta_flow_strips = true; // if true, require full phi coverage
  float min_tower_energy = -999; // if > 0, exclude towers with energy below this threshold from background estimation and jet finding
  bool renormalize_eta_strip = false; // if true, divide tower energies by average energy in strip to account for eta dependence of background
  bool do_double_check_on_UE = false; // if true, after flow modulation,


  ///! sets the embedding g4 flag used to determine which particles are classified as 'truth' in MakeHITruthJets
  ///! if you don't know what this is, you probably don't need to change it
  ///! 0 = all particles (use for all g4particles !not useful for HIJING+Pythia samples)
  ///! 1 = pythia/herwig particles only (use for pythia/herwig jets in HIJING+Pythia samples)
  ///! 2 = pythia particles from the HIJING+Pythia samples (use for pythia jets in HIJING+Pythia samples)
  ///! negative values are typically background particles (HIJING particles)
  int embedding_flag = 1;

  // do_flow = 0 --NoFlow
  // do_flow = 1 --psi2 derived from calo
  // do_flow = 2 --psi2 derived from HIJING
  // do_flow = 3 --psi2 derived from sEPD
  int do_flow = 0;

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

  // --------------------------------------------------------------------------
  //! Helper method to get towerinput object with appropriate settings
  // --------------------------------------------------------------------------
  TowerJetInput * TowerInput ( Jet::SRC src ) 
  {   
      auto input = new TowerJetInput(src, HIJETS::tower_prefix);
      if ( HIJETS::do_vertex_type )
      {
        input->set_GlobalVertexType(HIJETS::vertex_type);
      }
      return input;
  } // end 'TowerInput()'


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
  auto * se = Fun4AllServer::instance();

  // need the sepd if flow = 3
  if ( HIJETS::do_flow == 3 )
  {
    auto * epreco = new EventPlaneReco();
    epreco -> set_inputNode( HIJETS::EPD_tower_node );
    se -> registerSubsystem( epreco );
  }

  // retower emcal 
  auto * rcemc = new RetowerCEMC();
  rcemc -> set_towerNodePrefix( HIJETS::tower_prefix );
  rcemc -> Verbosity( verbosity );
  rcemc -> set_frac_cut( 0.5 );  
  rcemc -> set_towerinfo( true );
  se -> registerSubsystem( rcemc );

  // seed settings
  FastJetOptions seed_fj_opts( { Jet::ANTIKT, 0.2, 0 } );
  std::string seed_node_name = HIJETS::algo_prefix + "_TowerInfo_HIRecoSeedsRaw_r02";
  std::string seed_node_subname = HIJETS::algo_prefix + "_TowerInfo_HIRecoSeedsSub_r02";
  
  // input to subtraction and background estimation modules
  // defaults are retowered emcal, ihcal, emcal
  std::vector< Jet::SRC > raw_tower_inputs = {
    Jet::CEMC_TOWERINFO_RETOWER,
    Jet::HCALIN_TOWERINFO,
    Jet::HCALOUT_TOWERINFO
  };


  auto * towerjetreco = new JetReco( );
  for ( const auto & src : raw_tower_inputs ) { towerjetreco -> add_input( HIJETS::TowerInput( src ) ); }
  towerjetreco -> add_algo( new FastJetAlgoSub( seed_fj_opts ) , seed_node_name );
  towerjetreco -> set_algo_node( HIJETS::jet_node );
  towerjetreco -> set_input_node( "TOWER" );
  towerjetreco -> Verbosity( verbosity );
  se -> registerSubsystem( towerjetreco );

  auto * dtb = new DetermineTowerBackground();
  dtb -> SetBackgroundOutputName( "TowerInfoBackground_Sub1" );
  dtb -> SetFlow( HIJETS::do_flow );
  dtb -> SetSeedType( 0 );
  dtb -> SetSeedJetD( 3.0 );
  dtb -> SetSeedMaxConst( 3.0 );
  dtb -> SetSeedJetPt( 5.0 );
  dtb -> SetSeedExclusionDR( 0.4 );
  dtb -> UseReweighting( true );
  dtb -> set_towerNodePrefix( HIJETS::tower_prefix );
  dtb -> Verbosity( verbosity );
  se -> registerSubsystem( dtb );

  auto * casj = new CopyAndSubtractJets();
  casj -> SetFlowModulation( HIJETS::do_flow );
  casj -> set_rawseed_node( seed_node_name );
  casj -> set_subseed_node( seed_node_subname );
  casj -> set_background_node( "TowerInfoBackground_Sub1" );
  casj -> set_jet_node( HIJETS::jet_node );
  casj -> set_input_node( "TOWER" );
  casj -> set_towerinfo( true );
  casj -> set_towerNodePrefix( HIJETS::tower_prefix );
  casj -> Verbosity( verbosity );
  se   -> registerSubsystem( casj );

  auto * dtb2 = new DetermineTowerBackground();
  dtb2 -> SetBackgroundOutputName( "TowerInfoBackground_Sub2" );
  dtb2 -> SetFlow( HIJETS::do_flow );
  dtb2 -> SetSeedType( 1 );
  dtb2 -> SetSeedJetPt( 7.0 );
  dtb2 -> SetSeedExclusionDR( 0.4 );
  dtb2 -> UseReweighting( true );
  dtb2 -> set_towerNodePrefix( HIJETS::tower_prefix );
  dtb2 -> Verbosity( verbosity );
  se -> registerSubsystem( dtb2 );

  auto * st = new SubtractTowers( );
  st -> SetFlowModulation( HIJETS::do_flow );
  st -> Verbosity( verbosity );
  st -> set_towerinfo( true );
  st -> set_towerNodePrefix( HIJETS::tower_prefix );
  se -> registerSubsystem( st );


  // input to jet reconstruction (after subtraction)
  std::vector< Jet::SRC > sub_tower_inputs = {
    Jet::CEMC_TOWERINFO_SUB1,
    Jet::HCALIN_TOWERINFO_SUB1,
    Jet::HCALOUT_TOWERINFO_SUB1
  };
  std::vector< float > jet_Rs = { 0.2, 0.3, 0.4, 0.5 };
  
  towerjetreco = new JetReco( );
  for ( const auto & src : sub_tower_inputs ) { towerjetreco -> add_input( HIJETS::TowerInput( src ) ); }
  for ( const auto &R : jet_Rs ) { towerjetreco -> add_algo( HIJETS::GetFJAlgo(R), HIJETS::algo_prefix + "_Tower_r0" + std::to_string( static_cast<int>(R * 10) ) + "_Sub1" ); }
  towerjetreco -> set_algo_node( HIJETS::jet_node );
  towerjetreco -> set_input_node( "TOWER" );
  towerjetreco -> Verbosity( verbosity );
  se -> registerSubsystem( towerjetreco );

  return;

}

void MakeHITowerJetsv2()
{
  
  int verbosity = std::max(Enable::VERBOSITY, Enable::HIJETS_VERBOSITY);

  //---------------
  // Fun4All server
  //---------------
  auto * se = Fun4AllServer::instance();

  // need the sepd if flow = 3
  if ( HIJETS::do_flow == 3 )
  {
    auto * epreco = new EventPlaneReco();
    epreco -> set_inputNode( HIJETS::EPD_tower_node );
    se -> registerSubsystem( epreco );
  }

  // retower emcal 
  auto * rcemc = new RetowerCEMC();
  rcemc -> set_towerNodePrefix( HIJETS::tower_prefix );
  rcemc -> Verbosity( verbosity );
  rcemc -> set_frac_cut( 0.5 );  
  rcemc -> set_towerinfo( true );
  se -> registerSubsystem( rcemc );

  // seed settings
  FastJetOptions seed_fj_opts( { HIJETS::seed_algo, HIJETS::seed_R, 0 } );
  std::string seed_node_name = HIJETS::algo_prefix + "_TowerInfo_HIRecoSeedsRaw_r0" + std::to_string( static_cast<int>(HIJETS::seed_R * 10) );
  std::string seed_node_subname = HIJETS::algo_prefix + "_TowerInfo_HIRecoSeedsSub_r0" + std::to_string( static_cast<int>(HIJETS::seed_R * 10) );
  if ( HIJETS::seed_algo == Jet::KT )
  {
    seed_node_name = "kT_TowerInfo_HIRecoSeedsRaw_r0" + std::to_string( static_cast<int>(HIJETS::seed_R * 10) );
    seed_node_subname = "kT_TowerInfo_HIRecoSeedsSub_r0" + std::to_string( static_cast<int>(HIJETS::seed_R * 10) );
  }

  // input to subtraction and background estimation modules
  // defaults are retowered emcal, ihcal, emcal
  std::vector< Jet::SRC > raw_tower_inputs = {
    Jet::CEMC_TOWERINFO_RETOWER,
    Jet::HCALIN_TOWERINFO,
    Jet::HCALOUT_TOWERINFO
  };


  auto * towerjetreco = new JetReco( );
  for ( const auto & src : raw_tower_inputs ) { towerjetreco -> add_input( HIJETS::TowerInput( src ) ); }
  towerjetreco -> add_algo( new FastJetAlgoSub( seed_fj_opts ) , seed_node_name );
  towerjetreco -> set_algo_node( HIJETS::jet_node );
  towerjetreco -> set_input_node( "TOWER" );
  towerjetreco -> Verbosity( verbosity );
  se -> registerSubsystem( towerjetreco );

  auto * dtb = new DetermineTowerBackgroundv1();
  dtb -> SetBackgroundOutputName( "TowerInfoBackground_Sub1" );
  if ( HIJETS::do_flow == 0 )
  {
   dtb -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::NoFlow );
   dtb -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::NoPsi2 );
  }
  else if ( HIJETS::do_flow == 1 )
  {
   dtb -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::EBE );
   dtb -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::Calo );
  }
  else if ( HIJETS::do_flow == 2 )
  {
   dtb -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::EBE );
   dtb -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::Truth );
  }
  else if ( HIJETS::do_flow == 3 )
  {
   dtb -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::EBE );
   dtb -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::sEPD );
  }
  else if ( HIJETS::do_flow == 4 )
  {
    dtb -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::AVG );
    dtb -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::sEPD );
  }
  dtb -> SetSeedExclusionDR( HIJETS::DR );
  dtb -> UseReweighting( HIJETS::do_reweight );
  dtb -> ExcludeFullEtaFlowStrips( HIJETS::exclude_full_eta_flow_strips );
  dtb -> SetRenormalizeEtaStrip( HIJETS::renormalize_eta_strip );
  dtb -> SetDoDoubleCheckOnUE( HIJETS::do_double_check_on_UE );
  dtb -> SetMinTowerEnergy( HIJETS::min_tower_energy );  
  dtb -> SetNOmitSeeds( HIJETS::n_omit_seeds );
  dtb -> SetVertexType( HIJETS::vertex_type );
  dtb -> SetSeedType( 0 );
  dtb -> SetSeedJetD( 3.0 );
  dtb -> SetSeedMaxConst( 3.0 );
  dtb -> SetSeedJetPt( 5.0 );
  dtb -> SetIHCAL_TowerInfoNode( HIJETS::tower_prefix + "_HCALIN" );
  dtb -> SetOHCAL_TowerInfoNode( HIJETS::tower_prefix + "_HCALOUT" );
  dtb -> SetCEMC_RetowerInfoNode( HIJETS::tower_prefix + "_CEMC_RETOWER" );
  dtb -> SetSeedJetName( seed_node_name );
  dtb -> Verbosity( verbosity );
  se -> registerSubsystem( dtb );

  auto * casj = new CopyAndSubtractJets();
  casj -> SetFlowModulation( HIJETS::do_flow );
  casj -> set_rawseed_node( seed_node_name );
  casj -> set_subseed_node( seed_node_subname );
  casj -> set_background_node( "TowerInfoBackground_Sub1" );
  casj -> set_jet_node( HIJETS::jet_node );
  casj -> set_input_node( "TOWER" );
  casj -> set_towerinfo( true );
  casj -> set_towerNodePrefix( HIJETS::tower_prefix );
  casj -> Verbosity( verbosity );
  se   -> registerSubsystem( casj );
  
  auto * dtb2 = new DetermineTowerBackgroundv1();
  dtb2 -> SetBackgroundOutputName( "TowerInfoBackground_Sub2" );
  if ( HIJETS::do_flow == 0 )
  {
   dtb2 -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::NoFlow );
   dtb2 -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::NoPsi2 );
  }
  else if ( HIJETS::do_flow == 1 )
  {
   dtb2 -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::EBE );
   dtb2 -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::Calo );
  }
  else if ( HIJETS::do_flow == 2 )
  {
   dtb2 -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::EBE );
   dtb2 -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::Truth );
  }
  else if ( HIJETS::do_flow == 3 )
  {
   dtb2 -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::EBE );
   dtb2 -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::sEPD );
  }
  else if ( HIJETS::do_flow == 4 )
  {
    dtb2 -> SetFlowMode( DetermineTowerBackgroundv1::FlowMode::AVG );
    dtb2 -> SetPsi2Mode( DetermineTowerBackgroundv1::Psi2Mode::sEPD );
  }
  dtb2 -> SetSeedExclusionDR( HIJETS::DR );
  dtb2 -> UseReweighting( HIJETS::do_reweight );
  dtb2 -> ExcludeFullEtaFlowStrips( HIJETS::exclude_full_eta_flow_strips );
  dtb2 -> SetRenormalizeEtaStrip( HIJETS::renormalize_eta_strip );
  dtb2 -> SetDoDoubleCheckOnUE( HIJETS::do_double_check_on_UE );
  dtb2 -> SetMinTowerEnergy( HIJETS::min_tower_energy );
  dtb2 -> SetNOmitSeeds( HIJETS::n_omit_seeds );
  dtb2 -> SetVertexType( HIJETS::vertex_type );
  dtb2 -> SetSeedType( 1 );
  dtb2 -> SetSeedJetPt( 7.0 );
  dtb2 -> SetIHCAL_TowerInfoNode( HIJETS::tower_prefix + "_HCALIN" );
  dtb2 -> SetOHCAL_TowerInfoNode( HIJETS::tower_prefix + "_HCALOUT" );
  dtb2 -> SetCEMC_RetowerInfoNode( HIJETS::tower_prefix + "_CEMC_RETOWER" );
  dtb2 -> SetSeedJetName( seed_node_name );
  dtb2 -> Verbosity( verbosity );
  se -> registerSubsystem( dtb2 );

  auto * st = new SubtractTowers( );
  st -> SetFlowModulation( HIJETS::do_flow );
  st -> Verbosity( verbosity );
  st -> set_towerinfo( true );
  st -> set_towerNodePrefix( HIJETS::tower_prefix );
  se -> registerSubsystem( st );


  // input to jet reconstruction (after subtraction)
  std::vector< Jet::SRC > sub_tower_inputs = {
    Jet::CEMC_TOWERINFO_SUB1,
    Jet::HCALIN_TOWERINFO_SUB1,
    Jet::HCALOUT_TOWERINFO_SUB1
  };
  std::vector< float > jet_Rs = { 0.2, 0.3, 0.4, 0.5 };
  
  towerjetreco = new JetReco( );
  for ( const auto & src : sub_tower_inputs ) { towerjetreco -> add_input( HIJETS::TowerInput( src ) ); }
  for ( const auto &R : jet_Rs ) { towerjetreco -> add_algo( HIJETS::GetFJAlgo(R), HIJETS::algo_prefix + "_Tower_r0" + std::to_string( static_cast<int>(R * 10) ) + "_Sub1" ); }
  towerjetreco -> set_algo_node( HIJETS::jet_node );
  towerjetreco -> set_input_node( "TOWER" );
  towerjetreco -> Verbosity( verbosity );
  se -> registerSubsystem( towerjetreco );

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
  if (Enable::HIJETS_TOWER && !Enable::HIJETS_ENABLE_TEST ) MakeHITowerJets();
  if (Enable::HIJETS_TOWER && Enable::HIJETS_ENABLE_TEST ) MakeHITowerJetsv2();
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
