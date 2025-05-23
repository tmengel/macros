//#include <fun4all/Fun4AllDstOutputManager.h>
#include <fun4all/Fun4AllInputManager.h>
//#include <fun4all/Fun4AllOutputManager.h>
#include <fun4all/Fun4AllServer.h>

#include <fun4allraw/Fun4AllStreamingInputManager.h>
#include <fun4allraw/InputManagerType.h>
//#include <fun4allraw/SingleGl1PoolInput.h>
#include <fun4allraw/SingleMvtxPoolInput.h>

#include <ffamodules/CDBInterface.h>
#include <ffamodules/FlagHandler.h>
#include <ffamodules/HeadReco.h>
#include <ffamodules/SyncReco.h>

#include <ffarawmodules/StreamingCheck.h>

#include <phool/recoConsts.h>

#include <GlobalVariables.C>
//#include <Trkr_Reco.C>
//#include <Trkr_RecoInit.C>

#include <rawdatatools/RawDataManager.h>
#include <rawdatatools/RawDataDefs.h>
#include <rawdatatools/RawFileUtils.h>

#include <mvtxfhr/MvtxTriggerRampGaurd.h>
#include <mvtxfhr/MvtxFakeHitRate.h>

R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libfun4allraw.so)
R__LOAD_LIBRARY(libffamodules.so)
R__LOAD_LIBRARY(libffarawmodules.so)
R__LOAD_LIBRARY(librawdatatools.so)
R__LOAD_LIBRARY(libMvtxFHR.so)

void Fun4All_MvtxFHR(const int nEvents = 100000,
                     const int run_number = 42641,
                     const int trigger_rate_kHz = 44,
                     const std::string &output_name = "output.root",
                     const int target_layer = -1,
                     const std::string &trigger_guard_output_name = ""
                    )
{  
    

    bool run_trigger_guard_debug = false;
    if(trigger_guard_output_name != "")
    {
        run_trigger_guard_debug = true;
    }

    double trigger_rate = trigger_rate_kHz*1000.0;
    // const std::string output_name = "rootifles/fhr_calib_"+std::to_string(run_number)+"_"+std::to_string(trigger_rate_kHz)+"kHz_" + std::to_string(nEvents) + ".root";
    // const std::string output_name_triggerguard = "rootifles/trigger_guard_calib_"+std::to_string(run_number)+"_"+std::to_string(trigger_rate_kHz)+"kHz_" + std::to_string(nEvents) + ".root";
    const std::string run_type="calib";

    // raw data manager
    RawDataManager * rdm = new RawDataManager();
    rdm->SetDataPath("/sphenix/lustre01/sphnxpro/physics");
    //rdm->SetDataPath(RawDataDefs::SPHNXPRO_COMM);
    rdm->SetRunNumber(run_number);
    rdm->SetRunType(run_type);
    rdm->SelectSubsystems({RawDataDefs::MVTX});
    rdm->GetAvailableSubsystems();
    rdm->Verbosity(0);

    // Initialize fun4all
    Fun4AllServer *se = Fun4AllServer::instance();
    se->Verbosity(1);

    // Input manager
    recoConsts *rc = recoConsts::instance();
    Enable::CDB = true;
    rc->set_StringFlag("CDB_GLOBALTAG", CDB::global_tag);
    rc->set_uint64Flag("TIMESTAMP", run_number);
    rc->set_IntFlag("RUNNUMBER", run_number);

    Fun4AllStreamingInputManager *in = new Fun4AllStreamingInputManager("Comb");
    in->Verbosity(0);

    for(int iflx = 0; iflx < 6; iflx++)
    {
        SingleMvtxPoolInput  * mvtx_sngl = new SingleMvtxPoolInput("MVTX_FLX" + to_string(iflx));
        mvtx_sngl->Verbosity(0);
        mvtx_sngl->SetBcoRange(10);
        mvtx_sngl->SetNegativeBco(10);
        mvtx_sngl->runMVTXstandalone();
        std::vector<std::string> mvtx_files = rdm->GetFiles(RawDataDefs::MVTX, RawDataDefs::get_channel_name(RawDataDefs::MVTX, iflx));
        std::cout << "Adding files for flx " << iflx << std::endl;
        for (const auto &infile : mvtx_files)
        {
            if(!RawFileUtils::isGood(infile)){  continue; }
            std::cout << "\tAdding file: " << infile << std::endl;
            mvtx_sngl->AddFile(infile);
        }
        in->registerStreamingInput(mvtx_sngl, InputManagerType::MVTX);
    }
    se->registerInputManager(in);


    SyncReco *sync = new SyncReco();
    se->registerSubsystem(sync);

    HeadReco *head = new HeadReco();
    se->registerSubsystem(head);

    FlagHandler *flag = new FlagHandler();
    se->registerSubsystem(flag);

    //==================================================
    // Analysis modules
    MvtxTriggerRampGaurd *mtg = new MvtxTriggerRampGaurd();
    mtg->SetTriggerRate(trigger_rate_kHz*1000.0);
    mtg->SetConcurrentStrobeTarget(0);
    if(run_trigger_guard_debug)
    {
        mtg->SaveOutput(trigger_guard_output_name);
    }
    se->registerSubsystem(mtg);

    MvtxFakeHitRate *mfhr = new MvtxFakeHitRate();
    mfhr->SetOutputfile(output_name);
    mfhr->SetMaxMaskedPixels(100000);
    mfhr->SelectLayer(target_layer);
    se->registerSubsystem(mfhr);

    se->run(nEvents);
    se->End();
    delete se;
    gSystem->Exit(0);
    

}
