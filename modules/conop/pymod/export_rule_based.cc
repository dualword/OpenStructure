#include <boost/python.hpp>
#include <ost/conop/rule_based.hh>

using namespace boost::python;
using namespace ost::conop;

void export_rule_based()
{
  class_<RuleBasedProcessor>("RuleBasedProcessor", init<CompoundLibPtr>())
    .def("Process", &RuleBasedProcessor::Process)
  ;
}
