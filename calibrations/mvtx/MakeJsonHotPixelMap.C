#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>

#include <mvtxfhr/MvtxPixelDefs.h>

#include <trackbase/MvtxDefs.h>
#include <trackbase/TrkrDefs.h>

#include <TFile.h>
#include <TTree.h>

#include <nlohmann/json.hpp>

R__LOAD_LIBRARY(libMvtxFHR.so)
R__LOAD_LIBRARY(libfun4all.so)
R__LOAD_LIBRARY(libfun4allraw.so)
R__LOAD_LIBRARY(libffamodules.so)

using json = nlohmann::json;

class pixelParameters
{
  public:
    int layer;
    int stave;
    int chip;
    int row;
    int column;

    pixelParameters(int constructor_layer, int constructor_stave, int constructor_chip, int constructor_row, int constructor_column)
    {
      layer = constructor_layer;
      stave = constructor_stave;
      chip = constructor_chip;
      row = constructor_row;
      column = constructor_column;
    }
};

void MakeJsonHotPixelMap(const std::string & calibration_file="fhr_calib42641_100000.root",
                        const double target_threshold=10e-8)
{
    std::cout << "MakeJsonHotPixelMap - Reading calibration file " << calibration_file << std::endl;
    TFile * f = new TFile(calibration_file.c_str(), "READ");
    TTree * t = dynamic_cast<TTree*>(f->Get("masked_pixels"));
    int num_strobes = 0;
    int num_masked_pixels = 0;
    double noise_threshold = 0.0;
    std::vector<uint64_t> * masked_pixels = 0;
    t->SetBranchAddress("num_strobes", &num_strobes);
    t->SetBranchAddress("num_masked_pixels", &num_masked_pixels);
    t->SetBranchAddress("noise_threshold", &noise_threshold);
    t->SetBranchAddress("masked_pixels", &masked_pixels);

    int nentries = t->GetEntries();
    int selected_entry = -1;
    for (int i = 0; i < nentries; i++)
    {
        t->GetEntry(i);
        if(noise_threshold < target_threshold)
        {
            selected_entry = i;
            break;
        }
    }

    t->GetEntry(selected_entry);

    std::vector<pixelParameters> pixels_to_mask;

    for (int i = 0; i < num_masked_pixels; i++)
    {
        uint64_t pixel = masked_pixels->at(i);
        uint32_t hitsetkey = MvtxPixelDefs::get_hitsetkey(pixel);
        uint32_t hitkey = MvtxPixelDefs::get_hitkey(pixel);

        pixels_to_mask.push_back(pixelParameters(TrkrDefs::getLayer(hitsetkey), MvtxDefs::getStaveId(hitsetkey), MvtxDefs::getChipId(hitsetkey), MvtxDefs::getRow(hitkey), MvtxDefs::getCol(hitkey)));
    }
 

    std::cout << "Number of masked pixels: " << num_masked_pixels << std::endl;
    std::cout << "Noise threshold: " << noise_threshold << std::endl;

    std::cout << "Writing masked pixels to json file" << std::endl;

    json masked_pixels_file;

    for (auto& pixel : pixels_to_mask)
    {
        std::stringstream stave_sw_name;
        stave_sw_name << "L" << pixel.layer << "_" << std::setw(2) << std::setfill('0') << pixel.stave;
        json pixel_info = json::array({pixel.column, pixel.row});
        masked_pixels_file[stave_sw_name.str()][std::to_string(pixel.chip)] += pixel_info;
    }

    std::ofstream json_output("masked_pixels.json");
    json_output << masked_pixels_file.dump(2);
    json_output.close();

    gSystem->Exit(0);
}
