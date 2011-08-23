
// Boost Includes ==============================================================
#include <boost/python.hpp>
#include <boost/cstdint.hpp>

// Includes ====================================================================
#include <pyste/blitzblas_wrapper.cpp>

// Using =======================================================================
using namespace boost::python;

// Module ======================================================================
void Export_pyprop_core_pyste_blitzblas()
{
ExportBlitzBlas();
}
