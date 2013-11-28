#include <fstream>

#include "TCanvas.h"
#include "RooPlot.h"
#include "RooFormulaVar.h"
#include "RooMsgService.h"

#include "boost/lexical_cast.hpp"

#include "../interface/InitialFit.h"

using namespace std;
using namespace RooFit;

InitialFit::InitialFit(RooRealVar *massVar, RooRealVar *MHvar, int mhLow, int mhHigh, vector<int> skipMasses):
  mass(massVar),
  MH(MHvar),
  mhLow_(mhLow),
  mhHigh_(mhHigh),
	skipMasses_(skipMasses),
  verbosity_(0)
{
  allMH_ = getAllMH();
}

InitialFit::~InitialFit(){}

bool InitialFit::skipMass(int mh){
	for (vector<int>::iterator it=skipMasses_.begin(); it!=skipMasses_.end(); it++) {
		if (*it==mh) return true;
	}
	return false;
}

vector<int> InitialFit::getAllMH(){
  vector<int> result;
  for (int m=mhLow_; m<=mhHigh_; m+=5){
		if (skipMass(m)) continue;
    if (verbosity_>=1) cout << "LinearInterp - Adding mass: " << m << endl;
    result.push_back(m);
  }
  return result;
}

void InitialFit::setVerbosity(int v){
  if (v<1) {
    RooMsgService::instance().setGlobalKillBelow(RooFit::ERROR);
    RooMsgService::instance().setSilentMode(true);
  }
  verbosity_=v;
}

void InitialFit::setDatasets(map<int,RooDataSet*> data){
  datasets = data;
}

void InitialFit::addDataset(int mh, RooDataSet *data){
  assert(data);
  datasets.insert(pair<int,RooDataSet*>(mh,data));
}

void InitialFit::buildSumOfGaussians(string name, int nGaussians, bool recursive, bool forceFracUnity){

  for (unsigned int i=0; i<allMH_.size(); i++){
    int mh = allMH_[i];
    MH->setConstant(false);
    MH->setVal(mh);
    MH->setConstant(true);
    RooArgList *gaussians = new RooArgList();
    RooArgList *coeffs = new RooArgList();
    map<string,RooRealVar*> tempFitParams;
    map<string,RooAbsReal*> tempFitUtils;
    map<string,RooGaussian*> tempGaussians;
    
    for (int g=0; g<nGaussians; g++){
      RooRealVar *dm = new RooRealVar(Form("dm_mh%d_g%d",mh,g),Form("dm_mh%d_g%d",mh,g),0.1,-2.5*(1.+0.5*g),2.5*(1.+0.5*g));
      RooAbsReal *mean = new RooFormulaVar(Form("mean_mh%d_g%d",mh,g),Form("mean_mh%d_g%d",mh,g),"@0+@1",RooArgList(*MH,*dm));
      RooRealVar *sigma = new RooRealVar(Form("sigma_mh%d_g%d",mh,g),Form("sigma_mh%d_g%d",mh,g),2.,0.7,5.*(1.+0.5*g));
      RooGaussian *gaus = new RooGaussian(Form("gaus_mh%d_g%d",mh,g),Form("gaus_mh%d_g%d",mh,g),*mass,*mean,*sigma);
      tempFitParams.insert(pair<string,RooRealVar*>(string(dm->GetName()),dm));
      tempFitParams.insert(pair<string,RooRealVar*>(string(sigma->GetName()),sigma));
      tempFitUtils.insert(pair<string,RooAbsReal*>(string(mean->GetName()),mean));
      tempGaussians.insert(pair<string,RooGaussian*>(string(gaus->GetName()),gaus));
      gaussians->add(*gaus);
      if (g<nGaussians-1) {
        RooRealVar *frac = new RooRealVar(Form("frac_mh%d_g%d",mh,g),Form("frac_mh%d_g%d",mh,g),0.01,0.01,0.99);
        tempFitParams.insert(pair<string,RooRealVar*>(string(frac->GetName()),frac));
        coeffs->add(*frac);
      }
      if (g==nGaussians-1 && forceFracUnity){
        string formula="1.";
        for (int i=0; i<nGaussians-1; i++) formula += Form("-@%d",i);
        RooAbsReal *recFrac = new RooFormulaVar(Form("frac_mh%d_g%d",mh,g),Form("frac_mh%d_g%d",mh,g),formula.c_str(),*coeffs);
        tempFitUtils.insert(pair<string,RooAbsReal*>(string(recFrac->GetName()),recFrac));
        coeffs->add(*recFrac);
      }
    }
    assert(gaussians->getSize()==nGaussians && coeffs->getSize()==nGaussians-(1*!forceFracUnity));
    RooAddPdf *tempSumOfGaussians = new RooAddPdf(Form("%s_mh%d",name.c_str(),mh),Form("%s_mh%d",name.c_str(),mh),*gaussians,*coeffs,recursive);
    sumOfGaussians.insert(pair<int,RooAddPdf*>(mh,tempSumOfGaussians));
    fitParams.insert(pair<int,map<string,RooRealVar*> >(mh,tempFitParams));
    fitUtils.insert(pair<int,map<string,RooAbsReal*> >(mh,tempFitUtils));
    initialGaussians.insert(pair<int,map<string,RooGaussian*> >(mh,tempGaussians));
  }
}

void InitialFit::loadPriorConstraints(string filename, float constraintValue){

  ifstream datfile;
  datfile.open(filename.c_str());
  if (datfile.fail()) return;
  while (datfile.good()) {
    string line;
    getline(datfile,line);
    if (line=="\n" || line.substr(0,1)=="#" || line==" " || line.empty()) continue;
    string name = line.substr(0,line.find_first_of(" "));
    double val = boost::lexical_cast<double>(line.substr(line.find_first_of(" ")+1,string::npos));
    int mhS = boost::lexical_cast<int>(name.substr(name.find("_mh")+3,name.find("_g")-name.find("_mh")-3));
    if (verbosity_>=2) cout << name << " " << mhS << " " << val << endl;
    assert(fitParams.find(mhS)!=fitParams.end());
    assert(fitParams[mhS].find(name)!=fitParams[mhS].end());
    fitParams[mhS][name]->setVal(val);
    if (val>0.) fitParams[mhS][name]->setRange((1.-constraintValue)*val,(1.+constraintValue)*val);
    else fitParams[mhS][name]->setRange((1.+constraintValue)*val,(1.-constraintValue)*val);
  }
  datfile.close();
}

void InitialFit::saveParamsToFile(string filename){
  ofstream datfile;
  datfile.open(filename.c_str());
  for (unsigned int i=0; i<allMH_.size(); i++){
    int mh = allMH_[i];
    for (map<string,RooRealVar*>::iterator it=fitParams[mh].begin(); it!=fitParams[mh].end(); it++){
      datfile << Form("%s %1.5f",it->first.c_str(),it->second->getVal()) << endl;
    }
  }
  datfile.close();
}

void InitialFit::saveParamsToFileAtMH(string filename, int setMH){
  ofstream datfile;
  datfile.open(filename.c_str());
  for (unsigned int i=0; i<allMH_.size(); i++){
    int mh = allMH_[i];
    for (map<string,RooRealVar*>::iterator it=fitParams[setMH].begin(); it!=fitParams[setMH].end(); it++){
      string repName = it->first;
      repName = repName.replace(repName.find(Form("mh%d",setMH)),5,Form("mh%d",mh));
      datfile << Form("%s %1.5f",repName.c_str(),it->second->getVal()) << endl;
    }
  }
  datfile.close();
}

map<int,map<string,RooRealVar*> > InitialFit::getFitParams(){
  return fitParams;
}

void InitialFit::runFits(int ncpu){
  
  for (unsigned int i=0; i<allMH_.size(); i++){
    int mh = allMH_[i];
    MH->setConstant(false);
    MH->setVal(mh);
    MH->setConstant(true);
    assert(sumOfGaussians.find(mh)!=sumOfGaussians.end());
    assert(datasets.find(mh)!=datasets.end());
    RooAddPdf *fitModel = sumOfGaussians[mh];
    RooDataSet *data = datasets[mh];
		// help when dataset has no entries
		if (data->sumEntries()<1.e-6) {
			mass->setVal(mh);
			data->add(RooArgSet(*mass),1.e-6);
		}
    fitModel->Print();
    data->Print();
    RooFitResult *fitRes;
    verbosity_ >=3 ?
      fitRes = fitModel->fitTo(*data,NumCPU(ncpu),SumW2Error(true),Save(true)) :
      verbosity_ >=2 ?
        fitRes = fitModel->fitTo(*data,NumCPU(ncpu),SumW2Error(true),Save(true),PrintLevel(-1)) :
        fitRes = fitModel->fitTo(*data,NumCPU(ncpu),SumW2Error(true),Save(true),PrintLevel(-1),PrintEvalErrors(-1))
    ;
    fitResults.insert(pair<int,RooFitResult*>(mh,fitRes));
  }
}

void InitialFit::plotFits(string name){
  
  TCanvas *canv = new TCanvas();
  RooPlot *plot = mass->frame(Range(mhLow_-10,mhHigh_+10));
  for (unsigned int i=0; i<allMH_.size(); i++){
    int mh = allMH_[i];
    MH->setConstant(false);
    MH->setVal(mh);
    MH->setConstant(true);
    assert(sumOfGaussians.find(mh)!=sumOfGaussians.end());
    assert(datasets.find(mh)!=datasets.end());
    RooAddPdf *fitModel = sumOfGaussians[mh];
    RooDataSet *data = datasets[mh];
    data->plotOn(plot,Binning(160));
    fitModel->plotOn(plot);
  }
  plot->Draw();
  canv->Print(Form("%s.pdf",name.c_str()));
  canv->Print(Form("%s.png",name.c_str()));
  delete canv;
}
