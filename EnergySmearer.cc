#include "EnergySmearer.h"
#include "PhotonReducedInfo.h"
#include <assert.h>

EnergySmearer::EnergySmearer(const energySmearingParameters& par, const std::vector<PhotonCategory> & presel) : 
	myParameters_(par), scaleOrSmear_(true), doCorrections_(false), doRegressionSmear_(false),
	preselCategories_(presel)
{
  syst_only_ = false;
  rgen_ = new TRandom3(12345);
  baseSeed_ = 0;
  forceShift_ = false;
  name_="EnergySmearer_"+ par.categoryType + "_" + par.parameterSetName;
  //Checking consistency of input parameters
  std::cerr << myParameters_.categoryType << " " <<  myParameters_.n_categories << std::endl;
  assert( myParameters_.byRun || myParameters_.n_categories == myParameters_.smearing_sigma.size() );
  assert( myParameters_.byRun || myParameters_.n_categories == myParameters_.smearing_sigma_error.size() );
  assert( ( myParameters_.categoryType == "EBEE" && myParameters_.n_categories == 2 ) ||
	  ( myParameters_.categoryType == "2CatR9_EBEE" && myParameters_.n_categories == 4 ) ||
	  ( myParameters_.categoryType == "2CatR9_EBEE_ByRun" && myParameters_.n_categories == 4 ) ||
	  ( myParameters_.categoryType == "2CatR9_EBEBm4EE" && myParameters_.n_categories == 6 ) ||
	  ( myParameters_.categoryType == "2CatR9_EBEBm4EE_ByRun" && myParameters_.n_categories == 6 ) || 
	  ( myParameters_.categoryType == "Automagic" && 
	    ( myParameters_.byRun || myParameters_.n_categories == myParameters_.photon_categories.size() ) )
	  );
  if( myParameters_.categoryType == "Automagic" ) {
	  myParameters_.n_categories = myParameters_.photon_categories.size();
  }
  if( myParameters_.byRun ) {
    for(energySmearingParameters::eScaleVectorIt it=myParameters_.scale_offset_byrun.begin(); it!=myParameters_.scale_offset_byrun.end();
	++it ) {
	    assert( myParameters_.n_categories == it->scale_offset.size() );
	    assert( myParameters_.n_categories == it->scale_offset_error.size() );
    }
  } else {
    assert( myParameters_.n_categories == myParameters_.scale_offset.size() );
    assert( myParameters_.n_categories == myParameters_.scale_offset_error.size() );
  }
  
  registerMe();
}

EnergySmearer::EnergySmearer(EnergySmearer * orig, const std::vector<PhotonCategory> & presel)
{
    *this = *orig;
    preselCategories_ = presel;

    registerMe();
}


EnergySmearer::~EnergySmearer()
{
  delete rgen_;
}

std::string EnergySmearer::photonCategory(const energySmearingParameters & myParameters, const PhotonReducedInfo & aPho)
{
  std::string myCategory="";
  if (myParameters.categoryType=="Automagic") 
    {
	    EnergySmearer::energySmearingParameters::phoCatVectorConstIt vit = 
		find(myParameters.photon_categories.begin(), 
		     myParameters.photon_categories.end(),
		     PhotonCategory::photon_coord_t(
			     aPho.energy()/cosh(fabs((float)aPho.caloPosition().PseudoRapidity())),
			     fabs((float)aPho.caloPosition().PseudoRapidity()),(float)aPho.r9(),
			     aPho.isSphericalPhoton())
			);
	    if( vit ==  myParameters.photon_categories.end() ) {
		    std::cerr << "Could not find energy categoty for this photon " << 
		      aPho.isSphericalPhoton() << " " << (float)aPho.caloPosition().PseudoRapidity() << " " <<  (float)aPho.r9() << std::endl;
		    assert( 0 );
	    }
	    myCategory = vit->name;
    } 
  else if (myParameters.categoryType=="2CatR9_EBEE")
    {
      if (aPho.iDet()==1)
	myCategory+="EB";
      else
	myCategory+="EE";
      
      if (aPho.r9()>=0.94)
	myCategory+="HighR9";
      else
	myCategory+="LowR9";
    }
  else if (myParameters.categoryType=="2CatR9_EBEBm4EE")
    {
      if (aPho.iDet()==1 && fabs(aPho.caloPosition().PseudoRapidity())      < 1.)
	myCategory+="EB";
      else if (aPho.iDet()==1 && fabs(aPho.caloPosition().PseudoRapidity()) > 1.)
	myCategory+="EBm4";
      else
	myCategory+="EE";
      
      if (aPho.r9()>=0.94)
	myCategory+="HighR9";
      else
	myCategory+="LowR9";
    }
  else if (myParameters.categoryType=="EBEE")
    {
      if (aPho.iDet()==1)
	myCategory+="EB";
      else
	myCategory+="EE";
    }
  else
    {
      std::cout << "Unknown categorization. No category name is returned" << std::endl;
    }
  
  return myCategory;
}

float EnergySmearer::getSmearingSigma(const energySmearingParameters & myParameters, const std::string & category, 
				      float energy, float eta, float syst_shift)
{
  float smearing_sigma = myParameters.smearing_sigma.find(category)->second;
  float smearing_stocastic_sigma = myParameters.smearing_stocastic_sigma.find(category)->second;
  float smearing_stocastic_sigma_error = myParameters.smearing_stocastic_sigma_error.find(category)->second;
  float err_sigma= myParameters.smearing_sigma_error.find(category)->second;
  energySmearingParameters::parameterMapConstIt ipivot = myParameters.smearing_stocastic_pivot.find(category);
  
  if( myParameters.etStocastic ) {
	  energy /= cosh(eta);
  }
  smearing_sigma           += syst_shift * err_sigma;
  smearing_stocastic_sigma += syst_shift * smearing_stocastic_sigma_error;
  if( ipivot != myParameters.smearing_stocastic_pivot.end() ) {
	  float phi = std::max((float)0.,std::min((float)(TMath::Pi()*0.5),smearing_stocastic_sigma));
	  float rho = smearing_sigma;
	  smearing_stocastic_sigma = rho*ipivot->second*cos(phi);
	  smearing_sigma = rho*sin(phi);
  }
  smearing_stocastic_sigma = (smearing_stocastic_sigma * smearing_stocastic_sigma) / energy;
  smearing_sigma           = sqrt( smearing_sigma*smearing_sigma + smearing_stocastic_sigma );
  
  // Careful here, if sigma < 0 now, it will be squared and so not correct, set to 0 in this case.
  if (smearing_sigma < 0.) smearing_sigma=0.;
  return smearing_sigma;
}


std::string EnergySmearer::photonCategory(PhotonReducedInfo & aPho) const
{
  return photonCategory(myParameters_,aPho);
}


float EnergySmearer::getScaleOffset(int run, const std::string & category) const
{
  const std::map<std::string, float> * scale_offset =  &(myParameters_.scale_offset);
  
  if( myParameters_.byRun ) {
    scale_offset = &(find(myParameters_.scale_offset_byrun.begin(),myParameters_.scale_offset_byrun.end(),run)->scale_offset) ;
  }
  energySmearingParameters::parameterMapConstIt it=scale_offset->find(category);
  
  if ( it == scale_offset->end())
    {
      std::cout << "Category was not found in the configuration. Giving Up" << std::endl;
      return false;
    }
  
  return 1. + it->second;
  
}

bool EnergySmearer::smearPhoton(PhotonReducedInfo & aPho, float & weight, int run, float syst_shift) const
{
    if( syst_only_ && syst_shift == 0. ) { return true; }
    //// std::cout << "EnergySmearer::smearPhoton " << name() << " " << aPho.iPho() << " " << aPho.iDet() << " " << aPho.r9() << std::endl;
    if( ! forceShift_ && ! doEfficiencies_ && ! doCorrections_ && ! doRegressionSmear_ && syst_shift == 0. ) {
	    int myId = smearerId();
	    if( aPho.hasCachedVal(myId) ) {
		    const std::pair<const BaseSmearer *, float> & cachedVal = aPho.cachedVal(myId);
		    assert( cachedVal.first == this );
		    aPho.setEnergy(aPho.energy() * cachedVal.second );
		    //// std::cout << "Using cached value " << cachedVal.second << std::endl;
		    return true;
	    }
    }
    if( forceShift_ ) { forceShift_ = false; }
    if( ! preselCategories_.empty() ) {
	    if( find(preselCategories_.begin(), preselCategories_.end(),
		     PhotonCategory::photon_coord_t(
			     aPho.energy()/cosh(fabs((float)aPho.caloPosition().PseudoRapidity())),
			     fabs((float)aPho.caloPosition().PseudoRapidity()),(float)aPho.r9(),
			     aPho.isSphericalPhoton())
		    ) ==  preselCategories_.end() ) { 
		    //// std::cout << "Outside of this domain " << std::endl;
		    return true; 
	    }
    }
    
    std::string category=photonCategory(aPho);
    
    if (category == "")
    {
	std::cout << "No category has been found associated with this photon. Giving Up" << std::endl;
	return false;
    }
    
    /////////////////////// smearing or re-scaling photon energy ///////////////////////////////////////////
    float newEnergy=aPho.energy();

    /////////////////////// apply MC-based photon energy corrections ///////////////////////////////////////////
    if (  doCorrections_ ) {
	// corrEnergy is the corrected photon energy
	newEnergy = aPho.corrEnergy() + syst_shift * myParameters_.corrRelErr * (aPho.corrEnergy() - aPho.energy());
    } else if ( doRegressionSmear_){
	// leave energy alone, bus change resolution (10% uncertainty on sigmaE/E scaling)
        float newSigma;
	if (fabs(aPho.caloPosition().Eta())<1.5){
	    newSigma = aPho.rawCorrEnergyErr()*(1.+syst_shift*0.1);
	} else {
	    newSigma = aPho.rawCorrEnergyErr()*(1.+syst_shift*0.1);
	}
	aPho.setCorrEnergyErr(newSigma);
    } else {
	if( scaleOrSmear_ ) {
	    float scale_offset   = getScaleOffset(run, category);

	    scale_offset   += syst_shift * myParameters_.scale_offset_error.find(category)->second;
	    newEnergy *=  scale_offset;
	    if( syst_shift == 0. ) {
		    aPho.cacheVal( smearerId(), this, scale_offset );
	    }
	} else {
	  float smearing_sigma = getSmearingSigma( myParameters_, category, aPho.energy(), 
						   aPho.caloPosition().Eta(), syst_shift );
	  
	  float smear = 1.;
	  if( smearing_sigma > 0. ) {
	    // deterministic smearing
	    int nsigmas = round(syst_shift);
	    if( nsigmas < 0 ) nsigmas = 1-nsigmas;
	    if( nsigmas < aPho.nSmearingSeeds() ) {
	      rgen_->SetSeed( baseSeed_+aPho.smearingSeed(nsigmas) );
	    }
	    smear = rgen_->Gaus(1.,smearing_sigma) ;
	  }
	  if( syst_shift == 0. ) {
	    aPho.cacheVal( smearerId(), this, smear );
	  }
	  newEnergy *=  smear;
	}
    }
    if( newEnergy == 0. ) {
	std::cerr << "New energy is 0.: aborting " << this->name() << std::endl;
	assert( newEnergy != 0. );
    }
    aPho.setEnergy(newEnergy);
    
    /////////////////////// changing weigh of photon according to efficiencies ///////////////////////////////////////////
    //////////////////////  if you're doing corrections, don't touch the weights ////////////////////////////////////////
    if(doEfficiencies_ && (!doCorrections_) ) {
	if( !smearing_eff_graph_.empty()  ){
	    weight = getWeight( ( aPho.energy() / cosh(aPho.caloPosition().PseudoRapidity()) ) ,category, syst_shift);
	}
    }
    
    return true;
}



bool EnergySmearer::initEfficiency() 
{

  // if map is not empty, yuo're initilized and happy..
  if( !smearing_eff_graph_.empty() ){
     std:cout << "initialization of efficiencies already done; proceed with usage. " << std::endl;
    return true;
  }

  //otherwise, get smearing functions from file and set up map
  std::cout << "\n>>>initializing one efficiency for photon re-weighting; " <<  std::endl;
  
  // do basic sanity checks first
  if(doEnergy_){
    std::cout << "*** Initializing reweighting for efficiencies; energy smearing active TOO, do you want them both? " << std::endl;  return false; }
  if(!doEfficiencies_){
    std::cout << "*** Initializing reweighting for efficiencies - BUT doEfficiencies_ is set to false; doing nothing. " << std::endl;  return false; }
  if( effName_.empty()){
    std::cout << "you're initialzinfg reweighting for efficiency but effName_ is empty ; doing nothing. " << std::endl;  return false; }
  if( myParameters_.efficiency_file.empty()){
    std::cout << "you're initialzinfg reweighting for efficiency: " << effName_  << " but input file with TGraphErrors is not specified; doing nothing. " << std::endl;  return false; }
  
  theEfficiencyFile_ = new TFile(myParameters_.efficiency_file.c_str());

  // initialize formulas for the four categories; 
  std::string effTmpName; std::string photonCat; TGraphAsymmErrors *graphTmp, *graphClone;

  photonCat =  std::string("EBHighR9");
  effTmpName = effName_+std::string("_")+photonCat; graphTmp = (TGraphAsymmErrors*) theEfficiencyFile_->Get(effTmpName.c_str());   // smearing_eff_graph_[photonCat]=graphTmp;  
  graphClone=(TGraphAsymmErrors*)graphTmp->Clone();    smearing_eff_graph_[photonCat]=graphClone;    

  photonCat =  std::string("EBLowR9");
  effTmpName = effName_+std::string("_")+photonCat; graphTmp = (TGraphAsymmErrors*) theEfficiencyFile_->Get(effTmpName.c_str());   // smearing_eff_graph_[photonCat]=graphTmp;  
  graphClone=(TGraphAsymmErrors*)graphTmp->Clone();    smearing_eff_graph_[photonCat]=graphClone;    

  photonCat =  std::string("EEHighR9");
  effTmpName = effName_+std::string("_")+photonCat; graphTmp = (TGraphAsymmErrors*) theEfficiencyFile_->Get(effTmpName.c_str());   // smearing_eff_graph_[photonCat]=graphTmp;  
  graphClone=(TGraphAsymmErrors*)graphTmp->Clone();    smearing_eff_graph_[photonCat]=graphClone;    

  photonCat =  std::string("EELowR9");
  effTmpName = effName_+std::string("_")+photonCat; graphTmp = (TGraphAsymmErrors*) theEfficiencyFile_->Get(effTmpName.c_str());   // smearing_eff_graph_[photonCat]=graphTmp;  
  graphClone=(TGraphAsymmErrors*)graphTmp->Clone();    smearing_eff_graph_[photonCat]=graphClone;    

  delete  theEfficiencyFile_;
  return true;

}


double EnergySmearer::getWeight(double pt, std::string theCategory, float syst_shift) const
{
  std::map<std::string,TGraphAsymmErrors*>::const_iterator theIter = smearing_eff_graph_.find(theCategory);
  if( theIter != smearing_eff_graph_.end()  ) {

    // determine the pair of bins between which  you interpolate
    int numPoints = ( theIter->second )->GetN();
    double x, y;
    int myBin = -1;
    for (int bin=0; bin<numPoints; bin++ ){
      ( theIter->second )->GetPoint(bin, x, y);
      if(pt > x) {
	myBin = bin; }
      else break;
    }
    int binLow, binHigh; 
    if(myBin == -1)                      {binHigh = 0; binLow=0;}
    else if (myBin == (numPoints-1))     {binHigh = numPoints-1; binLow=numPoints-1;}
    else {binLow=myBin; binHigh=myBin+1;}


    // get hold of efficiency ratio and error at either points
    // low-high refer to the points ; up-down refers to the errors 
    double xLow, yLow;    double xHigh, yHigh;
    ( theIter->second )->GetPoint(binLow, xLow, yLow);
    ( theIter->second )->GetPoint(binHigh, xHigh, yHigh);

    double errLowYup    = ( theIter->second )->GetErrorYhigh(binLow);
    double errLowYdown  = ( theIter->second )->GetErrorYlow(binLow);
    double errHighYup   = ( theIter->second )->GetErrorYhigh(binHigh);
    double errHighYdown = ( theIter->second )->GetErrorYlow(binHigh);

    double theErrorLow, theErrorHigh;
    if(syst_shift>0) {theErrorLow = errLowYup;   theErrorHigh = errHighYup;}
    else             {theErrorLow = errLowYdown; theErrorHigh = errHighYdown;}
    
    double theWeight, theError;
    theWeight = yLow + (yHigh-yLow) / (xHigh-xLow) * (pt-xLow);
    theError  = theErrorLow + (theErrorHigh-theErrorLow) / (xHigh-xLow) * (pt-xLow);
     
    return  ( theWeight + (theError*syst_shift));
  }
  else {
    std::cout << "category asked: " << theCategory << " was not found - which is a problem. Returning weight 1. " << std::endl;
    return 1.;
  }
  
}

EnergySmearerExtrapolation::EnergySmearerExtrapolation(EnergySmearer * smearer) : 
	target_(smearer),
	name_(smearer->name()+"_extra"),
	myParameters_(smearer->myParameters_),
	needed_(false)
{
	for(EnergySmearer::energySmearingParameters::parameterMapConstIt ipivot = myParameters_.smearing_stocastic_pivot.begin(); 
	    ipivot != myParameters_.smearing_stocastic_pivot.end(); ++ipivot ) {
		if( ! target_->preselCategories_.empty() ) {
			bool presel = false;
			EnergySmearer::energySmearingParameters::phoCatVectorIt icat = find(myParameters_.photon_categories.begin(),
											    myParameters_.photon_categories.end(),
											    ipivot->first);
			assert( icat != myParameters_.photon_categories.end() );
			for( EnergySmearer::energySmearingParameters::phoCatVectorIt pcat=target_->preselCategories_.begin(); 
			     pcat!=target_->preselCategories_.end(); ++pcat) {
				if( *pcat > *icat ) {
					presel = true;
					break;
				}
			}
			if( ! presel ) { continue; }
		}
		std::cout << ipivot->first << " " << ipivot->second << std::endl;
		if( ipivot->second != 0.) { 			
			needed_ = true;
			break;
		}
	}
}


bool EnergySmearerExtrapolation::smearPhoton(PhotonReducedInfo &info, float & weight, int run, float syst_shift) const
{
	// modify the target smearer parameters such that the stocastic smearing correspond to syst_shift variations 
	//    from the nominal value
	std::string category = EnergySmearer::photonCategory(myParameters_,info);
	target_->myParameters_.smearing_stocastic_sigma_error[category] = 0.;
	target_->myParameters_.smearing_stocastic_sigma[category] = myParameters_.smearing_stocastic_sigma.find(category)->second 
		+ syst_shift*myParameters_.smearing_stocastic_sigma_error.find(category)->second;
	target_->forceShift_ = true;
	return true;
}
