#include "OSUT3Analysis/AnaTools/interface/EventVariableProducer.h"

EventVariableProducer::EventVariableProducer(const edm::ParameterSet &cfg) :
  collections_  (cfg.getParameter<edm::ParameterSet>  ("collections"))
{
  produces<EventVariableProducerPayload> ("extraEventVariables");
}

EventVariableProducer::~EventVariableProducer()
{
}

void
EventVariableProducer::produce (edm::Event &event, const edm::EventSetup &setup)
{
  // define structure that will be put into the event
  // string is the variable name
  // double is the value of the variable
  // it is a vector because  other collections are also vectors
  // the vector will have size=1 for each event
  auto_ptr<EventVariableProducerPayload> extraEventVariables (new EventVariableProducerPayload);

  ////////////////////////////////////////////////////////////////////////
  AddVariables(event, extraEventVariables);

  // store all of our calculated quantities in the event
  event.put (extraEventVariables, "extraEventVariables");
}

void
EventVariableProducer::AddVariables (const edm::Event &event, auto_ptr<EventVariableProducerPayload> &myVars) {}