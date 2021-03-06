
// Boost Includes ==============================================================
#include <boost/python.hpp>
#include <boost/cstdint.hpp>

// Includes ====================================================================
#include <transform/cartesianfouriertransform.h>

// Using =======================================================================
using namespace boost::python;

// Declarations ================================================================
#include <fftw3.h>


namespace  {

boost::shared_ptr<CartesianRepresentation<1> > (CartesianFourierTransform<1>::*CartesianFourierTransform_1___CreateFourierRepresentationconstCartesianRepresentation_1__)(const CartesianRepresentation<1>&)  = &CartesianFourierTransform<1>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<1> > (CartesianFourierTransform<1>::*CartesianFourierTransform_1___CreateFourierRepresentationconstCartesianRepresentation_1___int)(const CartesianRepresentation<1>&, int)  = &CartesianFourierTransform<1>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<2> > (CartesianFourierTransform<2>::*CartesianFourierTransform_2___CreateFourierRepresentationconstCartesianRepresentation_2__)(const CartesianRepresentation<2>&)  = &CartesianFourierTransform<2>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<2> > (CartesianFourierTransform<2>::*CartesianFourierTransform_2___CreateFourierRepresentationconstCartesianRepresentation_2___int)(const CartesianRepresentation<2>&, int)  = &CartesianFourierTransform<2>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<3> > (CartesianFourierTransform<3>::*CartesianFourierTransform_3___CreateFourierRepresentationconstCartesianRepresentation_3__)(const CartesianRepresentation<3>&)  = &CartesianFourierTransform<3>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<3> > (CartesianFourierTransform<3>::*CartesianFourierTransform_3___CreateFourierRepresentationconstCartesianRepresentation_3___int)(const CartesianRepresentation<3>&, int)  = &CartesianFourierTransform<3>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<4> > (CartesianFourierTransform<4>::*CartesianFourierTransform_4___CreateFourierRepresentationconstCartesianRepresentation_4__)(const CartesianRepresentation<4>&)  = &CartesianFourierTransform<4>::CreateFourierRepresentation;

boost::shared_ptr<CartesianRepresentation<4> > (CartesianFourierTransform<4>::*CartesianFourierTransform_4___CreateFourierRepresentationconstCartesianRepresentation_4___int)(const CartesianRepresentation<4>&, int)  = &CartesianFourierTransform<4>::CreateFourierRepresentation;


}// namespace 


// Module ======================================================================
void Export_python_cartesianfouriertransform()
{
    class_< CartesianFourierTransform<1>, boost::noncopyable >("CartesianFourierTransform_1", init<  >())
        .def("ForwardTransform", &CartesianFourierTransform<1>::ForwardTransform)
        .def("InverseTransform", &CartesianFourierTransform<1>::InverseTransform)
        .def("TransformRank", &CartesianFourierTransform<1>::TransformRank)
        .def("Renormalize", &CartesianFourierTransform<1>::Renormalize)
        .def("CreateFourierRepresentation", CartesianFourierTransform_1___CreateFourierRepresentationconstCartesianRepresentation_1__)
        .def("CreateFourierRepresentation", CartesianFourierTransform_1___CreateFourierRepresentationconstCartesianRepresentation_1___int)
    ;

    class_< CartesianFourierTransform<2>, boost::noncopyable >("CartesianFourierTransform_2", init<  >())
        .def("ForwardTransform", &CartesianFourierTransform<2>::ForwardTransform)
        .def("InverseTransform", &CartesianFourierTransform<2>::InverseTransform)
        .def("TransformRank", &CartesianFourierTransform<2>::TransformRank)
        .def("Renormalize", &CartesianFourierTransform<2>::Renormalize)
        .def("CreateFourierRepresentation", CartesianFourierTransform_2___CreateFourierRepresentationconstCartesianRepresentation_2__)
        .def("CreateFourierRepresentation", CartesianFourierTransform_2___CreateFourierRepresentationconstCartesianRepresentation_2___int)
    ;

    class_< CartesianFourierTransform<3>, boost::noncopyable >("CartesianFourierTransform_3", init<  >())
        .def("ForwardTransform", &CartesianFourierTransform<3>::ForwardTransform)
        .def("InverseTransform", &CartesianFourierTransform<3>::InverseTransform)
        .def("TransformRank", &CartesianFourierTransform<3>::TransformRank)
        .def("Renormalize", &CartesianFourierTransform<3>::Renormalize)
        .def("CreateFourierRepresentation", CartesianFourierTransform_3___CreateFourierRepresentationconstCartesianRepresentation_3__)
        .def("CreateFourierRepresentation", CartesianFourierTransform_3___CreateFourierRepresentationconstCartesianRepresentation_3___int)
    ;

    class_< CartesianFourierTransform<4>, boost::noncopyable >("CartesianFourierTransform_4", init<  >())
        .def("ForwardTransform", &CartesianFourierTransform<4>::ForwardTransform)
        .def("InverseTransform", &CartesianFourierTransform<4>::InverseTransform)
        .def("TransformRank", &CartesianFourierTransform<4>::TransformRank)
        .def("Renormalize", &CartesianFourierTransform<4>::Renormalize)
        .def("CreateFourierRepresentation", CartesianFourierTransform_4___CreateFourierRepresentationconstCartesianRepresentation_4__)
        .def("CreateFourierRepresentation", CartesianFourierTransform_4___CreateFourierRepresentationconstCartesianRepresentation_4___int)
    ;

def("fftw_import_wisdom_from_file", fftw_import_wisdom_from_file);
def("fftw_import_wisdom_from_string", fftw_import_wisdom_from_string);
}

