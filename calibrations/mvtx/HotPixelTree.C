#include <nlohmann/json.hpp>
#include <cdbobjects/CDBTTree.h>

using namespace std;
using json = nlohmann::json;

R__LOAD_LIBRARY(libcdbobjects.so)

void HotPixelTree(const std::string &fname = "masked_pixels_run_64639.root")
{
  CDBTTree *cdbttree = new CDBTTree(fname);


 //Mask Hot Pixels//

  unsigned int nStave[3] = {12, 16, 20};


  char *calibrationsroot = getenv("CALIBRATIONROOT");
  std::string deadpixelmap = "masked_pixels_3p2times10E-9_row_column_switch.json";
/*
  if (calibrationsroot != nullptr)
  {
	  deadpixelmap = std::string(calibrationsroot) + std::string("/Tracking/MVTX/") + deadpixelmap;
  }
*/
  std::ifstream f(deadpixelmap);
  json data = json::parse(f);
  int PixelIndex = 0;

  for (unsigned int layer = 0; layer < 3; ++layer)
  {
    for (unsigned int stave = 0; stave < nStave[layer]; ++stave)
    {
      std::stringstream staveStream;
      staveStream << "L" << layer << "_" << std::setw(2) << std::setfill('0') << stave;
      std::string staveName = staveStream.str();

      bool doesStaveExist = data.contains(staveName);
      if (!doesStaveExist) continue;

      for (unsigned int chip = 0; chip < 9; ++chip)
      {
        bool doesChipExist = data.at(staveName).contains(std::to_string(chip));
        if (!doesChipExist) continue;

        std::vector<std::pair<int, int>> dead_pixels = data.at(staveName).at(std::to_string(chip));
	
        for (auto &pixel : dead_pixels){ 

			cdbttree->SetIntValue(PixelIndex,"layer",layer);
			cdbttree->SetIntValue(PixelIndex,"stave",stave);
			cdbttree->SetIntValue(PixelIndex,"chip",chip);
                        cdbttree->SetIntValue(PixelIndex,"col",pixel.first);
			cdbttree->SetIntValue(PixelIndex,"row",pixel.second);
	                std::cout << "Layer: "<< layer << ", Stave: "<< stave << ", Chip: " << chip << ", Col: " << pixel.first << ", Row: " << pixel.second << std::endl;	
			PixelIndex = PixelIndex + 1;
		}
		
	
	  }
    }
  }
	

  cdbttree->Commit();
  

  cdbttree->SetSingleIntValue("TotalHotPixels",PixelIndex);

  cdbttree->CommitSingle();
 

  //cdbttree->SetSingleFloatValue("Test2",22);

  for (int i=0; i<10; i++)
  {
    string varname = "dvar" + to_string(i);
    //cdbttree->SetDoubleValue(23,varname,28875342.867);
  }
  //cdbttree->Print();
  cdbttree->WriteCDBTTree();
  delete cdbttree;
  gSystem->Exit(0);
}

CDBTTree *TestWrite(const std::string &fname = "test.root")
{
  CDBTTree *cdbttree = new CDBTTree(fname);
  cdbttree->SetSingleFloatValue("Test",25);
  cdbttree->SetSingleFloatValue("Test2",22);
  cdbttree->SetSingleFloatValue("Test3",23);
  cdbttree->SetSingleFloatValue("Test4",24);
  cdbttree->SetSingleIntValue("Tes2",24);
  cdbttree->SetSingleDoubleValue("Tes2",TMath::Pi());
  cdbttree->SetSingleUInt64Value("Tes2",12486346984672562);
  cdbttree->CommitSingle();
  cdbttree->SetFloatValue(2,"Tst",25);
  cdbttree->SetFloatValue(2,"Tt2",22);
  cdbttree->SetFloatValue(2,"Tes",23);
  cdbttree->SetFloatValue(2,"gaga",24);
  cdbttree->SetFloatValue(4,"Tst",5);
  cdbttree->SetFloatValue(4,"Tt2",2);
  cdbttree->SetFloatValue(4,"Tes",3);
  cdbttree->SetFloatValue(4,"Tara",7);
  cdbttree->SetIntValue(10,"blar",2864);
  cdbttree->SetUInt64Value(10,"blatr",28);
  for (int i=0; i<100; i++)
  {
    string varname = "dvar";
    string varname2 = "dvar2";
    string varname3 = "dvar3";
    string varname4 = "dvar4";
    for (int j=0; j<25; j++)
    {
      cdbttree->SetDoubleValue(j,varname,28875342.867*j);
      cdbttree->SetDoubleValue(j,varname2,2.867*j);
      cdbttree->SetDoubleValue(j,varname3,28875.8*j);
      cdbttree->SetDoubleValue(j,varname4,28875342*j);
    }
  }
  return cdbttree;
}

void Read(const std::string &fname = "test.root")
{
  CDBTTree *cdbttree = new CDBTTree(fname);
  cdbttree->LoadCalibrations();
  /*
  cout << "Test2: " << cdbttree->GetSingleFloatValue("Test2") << endl;
  cout << "Tt2(2): " << cdbttree->GetFloatValue(2,"Tt2") << endl;
  cout << "Tt2(4): " << cdbttree->GetFloatValue(4,"Tt2") << endl;
  cout << "blar: " << cdbttree->GetIntValue(10,"blar") << endl;
  cout << "int Tt2(4): " << cdbttree->GetIntValue(4,"Tt2") << endl;
  cout << "dvar5: " << cdbttree->GetDoubleValue(23,"dvar5");
  */

  int NPixel = -1;
  NPixel = cdbttree->GetSingleIntValue("TotalHotPixels");


  cout << "NPixel = " << NPixel << endl;

  for(int i = 0; i < NPixel; i++){

	  int Layer = cdbttree->GetIntValue(i,"layer");
	  int Stave = cdbttree->GetIntValue(i,"stave");
	  int Chip = cdbttree->GetIntValue(i,"chip");
	  int Col = cdbttree->GetIntValue(i,"col");
	  int Row = cdbttree->GetIntValue(i,"row");
	
	  cout << "Hot Pixel: " << i << "   Layer: " << Layer << "   Stave: " << Stave << "   Chip: " << Chip << "   Col: " << Col << "   Row: " << Row << endl;

  }

//  cdbttree->Print();
  delete cdbttree;
  gSystem->Exit(0);
}

