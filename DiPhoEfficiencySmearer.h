#ifndef __DIPHOEFFICIENCYSMEARER__
#define __DIPHOEFFICIENCYSMEARER__

#include "BaseSmearer.h"
#include "EfficiencySmearer.h"
#include <string>
#include <map>
#include "TFile.h"
#include "TGraphAsymmErrors.h"

class PhotonReducedInfo;
class TRandom3;

// ------------------------------------------------------------------------------------
class DiPhoEfficiencySmearer : public BaseDiPhotonSmearer
{
public:

  struct diPhoEfficiencySmearingParameters
  {
	  int n_categories;
	  std::string categoryType;
	  std::string parameterSetName;
	  
	  float idMVASystSize;

          std::string efficiency_file;
  };

  DiPhoEfficiencySmearer(const diPhoEfficiencySmearingParameters& par) ;
  virtual ~DiPhoEfficiencySmearer();
  
  virtual const std::string & name() const { return name_; };
  
  virtual bool smearDiPhoton( TLorentzVector & p4, TVector3 & selVtx, float & weight, const int & category, const int & genMassPoint, 
			      const TVector3 & trueVtx, float & idMVA1,float & idMVA2 ,float syst_shift) const ;

  void name(const std::string & x) { name_ = x; };

  void setEffName(const std::string & x) { effName_ =x; };

  bool init();
  void passFailWeights(bool x) { passFailWeights_ = x; };
  void doVtxEff(bool x) { doVtxEff_ = x; };
  void doMvaIdEff(bool x) { doMvaIdEff_ = x; };
  
  diPhoEfficiencySmearingParameters  myParameters_;
  
 protected:

  double getWeight(double pt, std::string theCategory, float syst_shift) const;

  bool passFailWeights_, doVtxEff_, doMvaIdEff_;
  float idMVASystSize_;
  
  std::string   name_;
  TRandom3     *rgen_;
  std::string   effName_;
  TFile        *theDiPhoEfficiencyFile_; 
  std::map<std::string,TGraphAsymmErrors*> smearing_eff_graph_;
};

#endif
