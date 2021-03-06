#include "OSUT3Analysis/AnaTools/interface/ObjectSelector.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#if IS_VALID(rpchits)
  typedef ObjectSelector<osu::Rpchit, TYPE(rpchits)> RpchitObjectSelector;
  DEFINE_FWK_MODULE(RpchitObjectSelector);
#endif
