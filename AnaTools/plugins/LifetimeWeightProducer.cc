#include <vector>
#include <unordered_set>
#include <sstream>

#include "OSUT3Analysis/AnaTools/plugins/LifetimeWeightProducer.h"

LifetimeWeightProducer::LifetimeWeightProducer(const edm::ParameterSet &cfg) :
  EventVariableProducer   (cfg),
  reweightingRules_       (cfg.getParameter<edm::VParameterSet> ("reweightingRules")),
  requireLastNotFirstCopy_(cfg.getParameter<bool> ("requireLastNotFirstCopy"))
{
  mcparticlesToken_ = consumes<vector<TYPE(hardInteractionMcparticles)> >(collections_.getParameter<edm::InputTag>("hardInteractionMcparticles"));

  dstCTau_.clear();
  srcCTau_.clear();
  pdgIds_.clear();
  isDefaultRule_.clear();

  stringstream suffix;
  for(unsigned int iRule = 0; iRule < reweightingRules_.size(); iRule++) {
    dstCTau_.push_back      (reweightingRules_[iRule].getParameter<vector<double> > ("dstCTaus"));
    srcCTau_.push_back      (reweightingRules_[iRule].getParameter<vector<double> > ("srcCTaus"));
    pdgIds_.push_back       (reweightingRules_[iRule].getParameter<vector<int> >    ("pdgIds"));
    isDefaultRule_.push_back(reweightingRules_[iRule].getParameter<bool>            ("isDefaultRule"));
    assert(srcCTau_.back().size() == dstCTau_.back().size() && 
           pdgIds_.back().size() == dstCTau_.back().size());

    suffix.str("");
    for(unsigned int iPdgId = 0; iPdgId < pdgIds_.back().size(); iPdgId++) {
      suffix << "_" << pdgIds_.back()[iPdgId] << "_" 
             << srcCTau_.back()[iPdgId] << "cmTo";
      if(dstCTau_.back()[iPdgId] < 1.0) {
        suffix << "0p" << dstCTau_.back()[iPdgId] * 10 << "cm";
      }
      else {
        suffix<< dstCTau_.back()[iPdgId] << "cm";
      }
    }
    
    weights_.push_back(1.0);
    weightNames_.push_back("lifetimeWeight" + suffix.str());
  }

}

LifetimeWeightProducer::~LifetimeWeightProducer() {
}

void
LifetimeWeightProducer::AddVariables(const edm::Event &event) {

#ifndef STOPPPED_PTLS
  for(unsigned int iRule = 0; iRule < weights_.size(); iRule++) weights_[iRule] = 1.0;

  edm::Handle<vector<TYPE(hardInteractionMcparticles)> > mcparticles;
  if(!event.getByToken(mcparticlesToken_, mcparticles)) {
    // Save the weights
    for(unsigned int iRule = 0; iRule < weights_.size(); iRule++) {
      (*eventvariables)[weightNames_[iRule]] = weights_[iRule];
      if(isDefaultRule_[iRule]) {
        (*eventvariables)["lifetimeWeight"] = weights_[iRule];
      }
    }
    return;
  }

  // The user supplies a VPSet of "rules", and an event weight is provided for each rule
  // Each rule is: a list of pdgIds, and (column-aligned with the pdgIds) lists of the
  // source and destination ctaus
  // Example rules:
  //     1000024: 100cm --> 90cm (produces lifetimeWeight_1000024_100cmTo90cm)
  //     1000006: 100cm --> 80cm (produces lifetimeWeight_1000006_100cmTo80cm)
  //     [1000024: 100cm --> 70cm, 1000022: 30cm --> 5cm] (produces lifetimeWeight_1000024_100cmTo70cm_1000022_30cmTo5cm)
  // and you can apply whichever of these you like in your protoConfig.

  for(unsigned int iRule = 0; iRule < dstCTau_.size(); iRule++) {

    vector<vector<double> > cTaus(pdgIds_[iRule].size());

    for(const auto &mcparticle: *mcparticles) {
      // Pythia8 first creates particles in the frame of the interaction, and only boosts them 
      // for ISR recoil in later steps. So only the last copy is desired. A particle that's both 
      // a last and first copy means it is a decay product created after the boost is applied,
      // e.g. a neutralino from a chargino decay, and we've already added the mother chargino.
      if(requireLastNotFirstCopy_) {
        if(!mcparticle.isLastCopy()) continue;
        if(mcparticle.statusFlags().isFirstCopy()) continue;
      }

      double cTau;
      for(unsigned int iPdgId = 0; iPdgId < pdgIds_[iRule].size(); iPdgId++) {
        if(abs(mcparticle.pdgId()) == abs(pdgIds_[iRule][iPdgId]) &&
           (requireLastNotFirstCopy_ || isOriginalParticle(mcparticle, mcparticle.pdgId())) &&
           (cTau = getCTau(mcparticle)) > 0.0) {
          cTaus[iPdgId].push_back(cTau);
        }
      }

    } // for mcparticles

    stringstream suffix;
    for(unsigned int iPdgId = 0; iPdgId < pdgIds_[iRule].size(); iPdgId++) {
      unsigned index = 0;
      for(const auto &cTau : cTaus[iPdgId]) {
        // Save the cTau for every mcparticle used for reweighting
        // If the same pdgId is used in multiple rules, this value will be
        // overwritten for every rule but the ordering/index is always the same, so it will be fine
        suffix.str("");
        suffix << "_" << abs(pdgIds_[iRule][iPdgId]) << "_" << index++;
        (*eventvariables)["cTau" + suffix.str()] = cTau;

        double srcPDF = exp(-cTau / srcCTau_[iRule][iPdgId]) / srcCTau_[iRule][iPdgId];
        double dstPDF = exp(-cTau / dstCTau_[iRule][iPdgId]) / dstCTau_[iRule][iPdgId];
        weights_[iRule] *= (dstPDF / srcPDF);
      }

      // Add dummy ctau values for unused values of "index" to guarantee these eventvariables exist.
      // That is, if your sample contains a variable number of charginos, still save INVALID_VALUE
      // for 10 charginos in case there ever could be that many.
      while(index < 10) {
        suffix.str("");
        suffix << "_" << abs(pdgIds_[iRule][iPdgId]) << "_" << index++;
        (*eventvariables)["cTau" + suffix.str()] = INVALID_VALUE;
      }
    } // for pdgIds in this rule

  } // for rules
#endif // ifndef STOPPPED_PTLS

  // Save the weights
  for(unsigned int iRule = 0; iRule < weights_.size(); iRule++) {
    (*eventvariables)[weightNames_[iRule]] = weights_[iRule];
    if(isDefaultRule_[iRule]) {
      (*eventvariables)["lifetimeWeight"] = weights_[iRule];
    }
  }
}

bool
LifetimeWeightProducer::isOriginalParticle(const TYPE(hardInteractionMcparticles) &mcparticle, const int pdgId) const
{
#ifndef STOPPPED_PTLS
  if(!mcparticle.numberOfMothers() || mcparticle.motherRef().isNull()) return true;
  return(mcparticle.motherRef()->pdgId() != pdgId);
#else
  return false;
#endif
}

double
LifetimeWeightProducer::getCTau(const TYPE(hardInteractionMcparticles) &mcparticle) const
{
#ifndef STOPPPED_PTLS
  math::XYZPoint v0 = mcparticle.vertex();
  math::XYZPoint v1;
  double boost = 1.0 /(mcparticle.p4().Beta() * mcparticle.p4().Gamma());

  getFinalPosition(mcparticle, mcparticle.pdgId(), true, v1);
  return((v1 - v0).r() * boost);
#else
  return 0.0;
#endif
}

// getFinalPosition: continue along the decay chain until you reach a particle with a different pdgId
//                   then take that particle's vertex
void
LifetimeWeightProducer::getFinalPosition(const reco::Candidate &mcparticle, const int pdgId, bool firstDaughter, math::XYZPoint &v1) const
{
#ifndef STOPPPED_PTLS
  if(mcparticle.pdgId() == pdgId) {
    v1 = mcparticle.vertex();
    firstDaughter = true;
  }
  else if(firstDaughter) {
    v1 = mcparticle.vertex();
    firstDaughter = false;
  }
  for(const auto &daughter : mcparticle) {
    getFinalPosition(daughter, pdgId, firstDaughter, v1);
  }
#endif
}

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(LifetimeWeightProducer);
