#ifndef __ENERGYSMEARER__
#define __ENERGYSMEARER__

#include "BaseSmearer.h"
#include <string>
#include <map>
#include <utility>
#include "TFile.h"
#include "TRandom3.h"
#include "TGraphAsymmErrors.h"

class PhotonReducedInfo;
class TRandom3;

// ------------------------------------------------------------------------------------
class EnergyScaleOffset {
public:
	EnergyScaleOffset(int first, int  last) : firstrun(first), lastrun(last) {};
	bool operator == (int run) const { return run>=firstrun && ( lastrun<0 || run<=lastrun); }; 
	bool operator == (const std::pair<int,int> & runrange) const { return runrange.first==firstrun && runrange.second==lastrun; }; 
	
	int firstrun,  lastrun;
	
	std::map<std::string,float> scale_offset;
	std::map<std::string,float> scale_offset_error;
	std::map<std::string,float> scale_stocastic_offset; // used for stocastic term smearing. Can also be reused in case of energy-dependend corrections
	std::map<std::string,float> scale_stocastic_offset_error;
	std::map<std::string,float> scale_stocastic_pivot;
	std::map<std::string,float> scale_stocastic_pivot_err;
};

class PhotonCategory {
public:
	enum photon_type_t { any=0, nogap=1, gap=2 };
	struct photon_coord_t
	{
		photon_coord_t(float et, float eta, float r9, bool isnogap) : 
			et_(et), eta_(eta), r9_(r9), isnogap_(isnogap)
			{};
		
		float et_, eta_, r9_;
		bool isnogap_;
	};
		
	PhotonCategory(float e1, float e2, float a, float b, 
		       float c, float d, photon_type_t e, std::string f ) : minet(e1), maxet(e2), mineta(a) , maxeta(b), minr9(c), maxr9(d), type(e), name(f) {};
	
	bool operator == (const PhotonCategory & rh) const { 
		return rh.type == type && 
			rh.mineta == mineta && rh.maxeta == maxeta && rh.minr9 == minr9 && rh.maxr9 == maxr9 
			&& rh.minet == minet && rh.maxet == maxet && rh.name == name; 
	}
	bool operator > (const PhotonCategory & rh) const { 
		return ( type == any || rh.type == type ) 
			&& rh.mineta >= mineta && rh.maxeta <= maxeta && rh.minr9 >= minr9 && rh.maxr9 <= maxr9 
			&& rh.minet >= minet && rh.maxet <= maxet;
	}
	bool operator == (const std::string & catname) const { return catname == name; }
	bool operator == (const photon_coord_t & photonCoordinates) const { 
		return ( type == any || ( type == nogap && photonCoordinates.isnogap_ || type == gap && ! photonCoordinates.isnogap_ ) ) &&
			photonCoordinates.eta_ >= mineta && photonCoordinates.eta_ <= maxeta && 
			photonCoordinates.r9_ >= minr9 && photonCoordinates.r9_ <= maxr9 &&
			photonCoordinates.et_ >= minet && photonCoordinates.et_ <= maxet
			; 
	};
	
	std::string name;
	float minet, maxet, mineta, maxeta, minr9, maxr9;
	photon_type_t type;
};

class EnergySmearerExtrapolation;

// ------------------------------------------------------------------------------------
class EnergySmearer : public BaseSmearer
{
public:
  friend class EnergySmearerExtrapolation;
  struct energySmearingParameters
  {
	  energySmearingParameters() : etStocastic(true) {};
	  
	  int n_categories;
	  bool byRun;
	  bool etStocastic;
	  std::string categoryType;
	  std::string parameterSetName;

	  typedef std::vector<PhotonCategory> phoCatVector;
	  typedef std::vector<PhotonCategory>::iterator phoCatVectorIt;
	  typedef std::vector<PhotonCategory>::const_iterator phoCatVectorConstIt;

	  typedef std::vector<EnergyScaleOffset> eScaleVector;
	  typedef std::vector<EnergyScaleOffset>::iterator eScaleVectorIt;
	  typedef std::vector<EnergyScaleOffset>::const_iterator eScaleVectorConstIt;
	  
	  typedef std::map<std::string,float> parameterMap;
	  typedef std::map<std::string,float>::iterator parameterMapIt;
	  typedef std::map<std::string,float>::const_iterator parameterMapConstIt;
	  
	  // Scale offset and smearing error should be espressed as a relative value
	  // Example: scale_offset["EB"]=1.002 , smearing_sigma["EB"]=0.01  
	  
	  std::map<std::string,float> scale_offset;
	  std::map<std::string,float> scale_offset_error;
	  std::map<std::string,float> scale_stocastic_offset;
	  std::map<std::string,float> scale_stocastic_offset_error;
	  std::map<std::string,float> scale_stocastic_pivot;
	  std::map<std::string,float> scale_stocastic_pivot_err;

	  std::map<std::string,float> smearing_sigma;
	  std::map<std::string,float> smearing_stocastic_sigma;
	  std::map<std::string,float> smearing_sigma_error;
	  std::map<std::string,float> smearing_stocastic_sigma_error;
	  // reference point for stocastic term extrapolation
	  //   \sigma(E) = \Delta S / E \oplus \Delta C = \rho * ( sin\phi * pivot \oplus cos\phi )
	  // if pivot != 0, then smearing_sigma and smearing_stocastic_sigma are interpreted as rho and phi
	  //    respectively
	  std::map<std::string,float> smearing_stocastic_pivot;
	  
	  phoCatVector photon_categories;
	  eScaleVector scale_offset_byrun;
	  
          std::string efficiency_file;
          // errors on correction will be a fraction of the correction itself   
          float       corrRelErr;
  };
  
  EnergySmearer(EnergySmearer * orig, const std::vector<PhotonCategory> & presel=std::vector<PhotonCategory>());

  EnergySmearer(const energySmearingParameters& par, const std::vector<PhotonCategory> & presel=std::vector<PhotonCategory>());
  virtual ~EnergySmearer();
  
  virtual const std::string & name() const { return name_; };
  
  virtual bool smearPhoton(PhotonReducedInfo &, float & weight, int run, float syst_shift) const;
  float getScaleOffset(int run, const std::string & category) const;

  void name(const std::string & x) { name_ = x; };
  
  void scaleOrSmear(bool x) { scaleOrSmear_=x; }; 

  void doEnergy(bool x) { doEnergy_=x; }; 

  void doRegressionSigma(bool x) { doRegressionSmear_=x; }; 

  void doCorrections(bool x) { doCorrections_=x; }; 
   
  void doEfficiencies(bool x) { doEfficiencies_=x; }; 
  
  void setEffName(std::string x) { effName_ =x; };

  void resetRandom(){rgen_->SetSeed(12345);};

  bool initEfficiency();
  
  void syst_only(bool x) { syst_only_ = x; };

  energySmearingParameters  myParameters_;
  
  std::string photonCategory(PhotonReducedInfo &) const;
  static std::string photonCategory(const energySmearingParameters &, const PhotonReducedInfo &);
  static float getSmearingSigma(const energySmearingParameters & myParameters, const std::string & category, float energy, 
				float eta, float syst_shift);
  
 protected:
  bool doEnergy_, scaleOrSmear_, doEfficiencies_, doCorrections_, doRegressionSmear_;
  mutable bool forceShift_;
  int baseSeed_;

  std::vector<PhotonCategory> preselCategories_;
  
  double getWeight(double pt, std::string theCategory, float syst_shift) const;
  
  std::string   name_;
  TRandom3     *rgen_;
  std::string   effName_;
  TFile        *theEfficiencyFile_; 
  bool syst_only_;
  std::map<std::string,TGraphAsymmErrors*> smearing_eff_graph_;
};



class EnergySmearerExtrapolation : public BaseSmearer
{
public:
	EnergySmearerExtrapolation(EnergySmearer * smearer);
	
	virtual bool smearPhoton(PhotonReducedInfo &, float & weight, int run, float syst_shift) const;
	virtual const std::string & name() const { return name_; };

	bool needed() { return needed_; };
private:
	EnergySmearer * target_;
	std::string name_;
	bool needed_;
	EnergySmearer::energySmearingParameters  myParameters_;

};


#endif
