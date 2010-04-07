
// Boost Includes ==============================================================
#include <boost/python.hpp>
#include <boost/cstdint.hpp>

// Includes ====================================================================
#include <src/potential.h>

// Using =======================================================================
using namespace boost::python;

// Module ======================================================================
void Export_pyprop_modules_potentials_oscillator_src_potential()
{
    class_< DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>, boost::noncopyable >("DynamicPotentialEvaluator_HarmonicOscillatorPotential_1_1", init<  >())
        .def("ApplyConfigSection", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::ApplyConfigSection)
        .def("ApplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::ApplyPotential)
        .def("MultiplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::MultiplyPotential)
        .def("UpdateStaticPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::UpdateStaticPotential)
        .def("GetPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::GetPotential)
        .def("UpdatePotentialData", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::UpdatePotentialData)
        .def("CalculateExpectationValue", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<1>,1>::CalculateExpectationValue)
    ;

    class_< DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>, boost::noncopyable >("DynamicPotentialEvaluator_HarmonicOscillatorPotential_2_2", init<  >())
        .def("ApplyConfigSection", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::ApplyConfigSection)
        .def("ApplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::ApplyPotential)
        .def("MultiplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::MultiplyPotential)
        .def("UpdateStaticPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::UpdateStaticPotential)
        .def("GetPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::GetPotential)
        .def("UpdatePotentialData", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::UpdatePotentialData)
        .def("CalculateExpectationValue", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<2>,2>::CalculateExpectationValue)
    ;

    class_< DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>, boost::noncopyable >("DynamicPotentialEvaluator_HarmonicOscillatorPotential_3_3", init<  >())
        .def("ApplyConfigSection", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::ApplyConfigSection)
        .def("ApplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::ApplyPotential)
        .def("MultiplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::MultiplyPotential)
        .def("UpdateStaticPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::UpdateStaticPotential)
        .def("GetPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::GetPotential)
        .def("UpdatePotentialData", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::UpdatePotentialData)
        .def("CalculateExpectationValue", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<3>,3>::CalculateExpectationValue)
    ;

    class_< DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>, boost::noncopyable >("DynamicPotentialEvaluator_HarmonicOscillatorPotential_4_4", init<  >())
        .def("ApplyConfigSection", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::ApplyConfigSection)
        .def("ApplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::ApplyPotential)
        .def("MultiplyPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::MultiplyPotential)
        .def("UpdateStaticPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::UpdateStaticPotential)
        .def("GetPotential", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::GetPotential)
        .def("UpdatePotentialData", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::UpdatePotentialData)
        .def("CalculateExpectationValue", &DynamicPotentialEvaluator<HarmonicOscillatorPotential<4>,4>::CalculateExpectationValue)
    ;

}

