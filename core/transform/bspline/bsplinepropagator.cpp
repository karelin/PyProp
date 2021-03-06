#include "bsplinepropagator.h"
#include "../representation/representation.h"
#include "../representation/reducedspherical/reducedsphericalharmonicrepresentation.h"
#include "../../utility/blitzblas.h"
#include "../../utility/blitztricks.h"
#include "../../utility/blitzlapack.h"
#include <cmath>


namespace BSpline
{

template<int Rank>
void Propagator<Rank>::ApplyConfigSection(const ConfigSection &config)
{
	config.Get("mass", Mass);
	cout << "BSplinePropagator: Mass = " << Mass << endl;
	
	if (config.HasValue("propagation_algorithm") )
	{
		config.Get("propagation_algorithm", PropagationAlgorithm);
		cout << "BSplinePropagator: using algorithm " << PropagationAlgorithm << endl;
	}

	if (config.HasValue("lmax") )
	{
		config.Get("lmax", GlobalLMax);
		cout << "BSplinePropagator: global lmax = " << GlobalLMax << endl;
	}

	if ( config.HasValue("angular_rank") )
	{
		config.Get("angular_rank", AngularRank);
	}

}

/*
 * Set up propagator
 */
template<int Rank>
void Propagator<Rank>::Setup(const cplx &dt, const Wavefunction<Rank> &psi, 
	BSpline::Ptr bsplineObject, int rank)
{
	using namespace blitz;

	TimeStep = dt;

	//Get b-spline object from wavefunction
	BSplineObject = bsplineObject;

	//Set class parameters
	PropagateRank = rank;

	//Call setup routine to calculate all LAPACK form matrices
	SetupLapackMatrices(dt);

	//Call setup routine to calculate all BLAS form matrices
	SetupBlasMatrices(dt);

	//Allocate temp data
	TempData.resize(psi.GetData().extent(rank));
}

/*
 * Set up propagator with additional potential
 */
template<int Rank>
void Propagator<Rank>::Setup(const cplx &dt, const Wavefunction<Rank> &psi, 
	BSpline::Ptr bsplineObject, blitz::Array<double, 1> potential, int rank)
{
	//Get b-spline potential values
	PotentialVector.reference( potential.copy() );

	//We have a b-spline potential
	HasPotential = true;

	//Setup "the rest"
	Setup(dt, psi, bsplineObject, rank);
}


/*
 * Set up centrifugal term
 */
template<int Rank>
void Propagator<Rank>::SetupCentrifugalPotential(blitz::Array< double, 1> centrifugalPotential, 
	const Wavefunction<Rank> &psi)
{
	//Get centrifugal potential values
	CentrifugalVector.reference( centrifugalPotential.copy() );

	//Get local l-range from distribution
	LocalLRange = 
		psi.GetRepresentation()->GetDistributedModel()->GetLocalIndexRange(GlobalLMax + 1, AngularRank);

	//We have a centrifugal term
	HasCentrifugalPotential = true;
}


/*
 * Advance the wavefunction one time step
 */
template<int Rank>
void Propagator<Rank>::AdvanceStep(Wavefunction<Rank> &psi)
{
	using namespace blitz;

	//Map the data to a 3D array, where the b-spline part is the middle rank
	Array<cplx, 3> data3d = MapToRank3(psi.Data, PropagateRank, 1);

	//Propagate the 3D array
	ApplyCrankNicolson(PropagationMatrix, data3d);
}


/*
 * Multiply hamiltonian on wavefunction. The non-orthogonality of b-splines
 * introduces the overlap matrix into this, requiring us to solve
 * 
 *     S * d = H * c = b
 *
 *  where d are the new wavefunction coefficients. A BLAS call takes care of
 *  the matrix-vector product H * c, and then we use LAPACK to solve S * d = b.
 */
template<int Rank>
void Propagator<Rank>::MultiplyHamiltonian(Wavefunction<Rank> &srcPsi, Wavefunction<Rank> &dstPsi)
{
	using namespace blitz;
	linalg::LAPACK<cplx> lapack;
	
	//Map the data to a 3D array, where the b-spline part is the middle rank
	Array<cplx, 3> srcData = MapToRank3(srcPsi.Data, PropagateRank, 1);
	Array<cplx, 3> dstData = MapToRank3(dstPsi.Data, PropagateRank, 1);
	Array<cplx, 2> matrix = GetHamiltonianMatrix();
	Array<cplx, 2> centrifugalMatrix = GetCentrifugalMatrixBlas();

	//Iterate over the array directions which are not propagated by this propagator
	Array<cplx, 1> temp(srcData.extent(1));
	for (int i = 0; i < srcData.extent(0); i++)
	{
		for (int j = 0; j < srcData.extent(2); j++)
		{
			Array<cplx, 1> v = srcData(i, Range::all(), j);
			Array<cplx, 1> w = dstData(i, Range::all(), j);
			MatrixVectorMultiplyHermitianBanded(matrix, v, temp, 1.0, 0.0);

			if (HasCentrifugalPotential)
			{
				//Get local lmin
				int lmin = LocalLRange.first();
	
				double l = j + lmin;
				double factor = l * (l + 1.0) / (2.0 * Mass);
				MatrixVectorMultiplyHermitianBanded(centrifugalMatrix, v, temp, factor, 1.0);
			}

			//Then call LAPACK to solve system of equations
			//    S * d = b
			BSplineObject->SolveForOverlapMatrix(temp);

			//Add solution to w in case psi is more than rank 1
			w += temp;
		}
	}
}


/*
 * Propagate the b-spline rank of the wavefunction using the Crank-Nicolson method.
 * Since the b-splines are not orthogonal, the usual identity matrix appearing in
 * the formula is replaced by the overlap matrix S. This matrix is sparse (2k - 1 bands),
 * and we utilize this by first calling a BLAS function to do the banded matrix-vector
 * multiplication on the right, before calling LAPACK to solve the system of equation.
 *
 *     M * c(t+dt/2) = b(t-dt/2)  (solve using LAPACK)
 *     b = conj(M) * c(t-dt/2)    (perform using BLAS)
 *
 * where
 *
 *     M = S + idt/2 * H
 * 
 */
template<int Rank>
void Propagator<Rank>::ApplyCrankNicolson(const blitz::Array<cplx, 2> &matrix, 
	blitz::Array<cplx, 3> &data)
{
	using namespace blitz;
	linalg::LAPACK<cplx> lapack;

	Array<cplx, 1> temp = TempData;

	int N = BSplineObject->NumberOfBSplines;
	int offDiagonalBands = BSplineObject->MaxSplineOrder - 1;
	
	cplx scaling = -I * TimeStep / 2.0;


	if (PropagationAlgorithm == 0)
	{
		Array<int, 1> pivot(N);
		Array<cplx, 2> pmat(PropagationMatrix.shape());
	
		//Iterate over the array directions which are not propagated by this propagator
		for (int i=0; i<data.extent(0); i++)
		{
			for (int j=0; j<data.extent(2); j++)
			{
				/* 
				 * pmat is a copy of the propagation matrix. Need to copy this 
				 * every iteration since LAPACK call destroys input matrix.
				 */
				pmat = PropagationMatrix;

				/* 
				 * v is a view of a slice of the wave function
				 * temp is a temporary copy of that slice
				 */
				Array<cplx, 1> v = data(i, Range::all(), j);
				temp = v; 

				/*
				 * Call BLAS function for fast banded matrix-vector product.
				 * While both the overlap and hamilton matrices are symmetric,
				 * this is not so for the propagation matrix S + idt/2 * H.
				 * Since we are using the banded hermitian BLAS matrix-vector
				 * product routine, we multiply in two steps and sum up implicitly
				 * in BLAS using a scaling parameter.
				 */
				MatrixVectorMultiplyHermitianBanded(HamiltonianMatrix, temp, v, scaling, 0.0);
				MatrixVectorMultiplyHermitianBanded(OverlapMatrixBlas, temp, v, 1.0, 1.0);

				if (HasCentrifugalPotential)
				{
					//Get local lmin
					int lmin = LocalLRange.first();

					double l = lmin + j;
					cplx factor = l * (l + 1.0) / (2.0 * Mass) * scaling;
					MatrixVectorMultiplyHermitianBanded(CentrifugalMatrixBlas, temp, v, factor, 1.0);

					pmat -= factor * CentrifugalMatrix;
				}

				/*
				 * Then call LAPACK to solve banded system of equations, obtaining 
				 * solution at t+dt/2. We must a temp array copy of v to get the
				 * stride right for LAPACK (stride = 1)
				 */
				temp = v;
				pivot = 0;
				lapack.SolveGeneralBandedSystemOfEquations(pmat, pivot, temp, offDiagonalBands,
					offDiagonalBands);
				//lapack.CalculateLUFactorizationBanded(pmat, pivot);
				//lapack.SolveBandedFactored(pmat, pivot, temp);

				v = temp;
			}
		}
	}

	else if (PropagationAlgorithm == 1)
	
	{
		//Find local lmin
		int lmin = LocalLRange.first();

		//Iterate over the array directions which are not propagated by this propagator
		for (int i=0; i<data.extent(0); i++)
		{
			for (int j=0; j<data.extent(2); j++)
			{
				/* 
				 * v is a view of a slice of the wave function
				 * temp is a temporary copy of that slice
				 */
				Array<cplx, 1> v = data(i, Range::all(), j);
				temp = v; 

				/*
				 * Call BLAS function for fast banded matrix-vector product.
				 * While both the overlap and hamilton matrices are symmetric,
				 * this is not so for the propagation matrix S + idt/2 * H.
				 * Since we are using the banded hermitian BLAS matrix-vector
				 * product routine, we multiply in two steps and sum up implicitly
				 * in BLAS using a scaling parameter.
				 */
				MatrixVectorMultiplyHermitianBanded(HamiltonianMatrix, temp, v, scaling, 0.0);
				MatrixVectorMultiplyHermitianBanded(OverlapMatrixBlas, temp, v, 1.0, 1.0);

				if (HasCentrifugalPotential)
				{
					double l = j + lmin;
					cplx factor = l * (l + 1.0) / (2.0 * Mass) * scaling;
					MatrixVectorMultiplyHermitianBanded(CentrifugalMatrixBlas, temp, v, factor, 1.0);
				}

				/*
				 * Then call LAPACK to solve banded system of equations, obtaining 
				 * solution at t+dt/2. We must make a temp array copy of v to get the
				 * stride right for LAPACK (stride = 1)
				 */
				temp = v;
				lapack.SolveBandedFactored(BigPropagationMatrix(j), Pivots(j), temp);
				v = temp;
			}
		}	
	}

	else if (PropagationAlgorithm == 2)
	
	{
		//Iterate over the array directions which are not propagated by this propagator
		for (int i=0; i<data.extent(0); i++)
		{
			for (int j=0; j<data.extent(2); j++)
			{
				/* 
				 * v is a view of a slice of the wave function
				 * temp is a temporary copy of that slice
				 */
				Array<cplx, 1> v = data(i, Range::all(), j);
				temp = v; 

				/*
				 * Call BLAS function for fast banded matrix-vector product.
				 * While both the overlap and hamilton matrices are symmetric,
				 * this is not so for the propagation matrix S + idt/2 * H.
				 * Since we are using the banded hermitian BLAS matrix-vector
				 * product routine, we multiply in two steps and sum up implicitly
				 * in BLAS using a scaling parameter.
				 */
				MatrixVectorMultiplyHermitianBanded(HamiltonianMatrix, temp, v, scaling, 0.0);
				MatrixVectorMultiplyHermitianBanded(OverlapMatrixBlas, temp, v, 1.0, 1.0);

				/*
				 * Then call LAPACK to solve banded system of equations, obtaining 
				 * solution at t+dt/2. We must make a temp array copy of v to get the
				 * stride right for LAPACK (stride = 1)
				 */
				temp = v;
				lapack.SolveBandedFactored(BigPropagationMatrix(0), Pivots(0), temp);
				v = temp;
			}
		}	
	}

	else

	{
		cout << "ERROR: Unknown propagation algorithm " << PropagationAlgorithm << endl;
	}
}


/*
 * Set up matrices to be stored on LAPACK form. Three algorithms are available,
 * two of which only applies in special cases:
 *
 * Algo 0: Slow, but works for all cases
 * Algo 1: Optimized, only works for 2D reducedspherical representation (sphRank = 1)
 * Algo 2: Optimized, works when there are only RankOne b-spline potential present 
 *
 * Algo 1 and 2 performs an initial LU factorization of the propagation matrix, providing
 * approx x2 speedup over algo 0 (sometimes more).
 */
template<int Rank>
void Propagator<Rank>::SetupLapackMatrices(const cplx &dt)
{
	using namespace blitz;
	linalg::LAPACK<cplx> lapack;

	int k = BSplineObject->MaxSplineOrder;
	int N = BSplineObject->NumberOfBSplines;
	int ldab = 3 * k - 2;
	
	/*
	 * Get full b-bspline overlap matrix from b-bspline object,
	 * since it is already stored on LAPACK form
	 */

	//Array to hold slices of a RankOne potential, if present
	Array<double, 1> potentialSlice(BSplineObject->GetBSpline(0).extent(0));


	if (PropagationAlgorithm == 0)
	{
		//Resize centrifugal matrix is applicable
		if (HasCentrifugalPotential) { CentrifugalMatrix.resize(N, ldab); }
		
		//Resize prop matrix
		PropagationMatrix.resize(N, ldab);

		cplx ham = 0.0;
		cplx overlap = 0.0;
		cplx centrifugal = 0.0;
		for (int i = 0; i < N; i++)
		{
			int jMax = std::min(i + k, N);
			for (int j = i; j < jMax; j++)
			{
				//Upper lapack indices
				int Ju = j;
				int Iu = 2 * k - 2 + i - j;

				//Lower lapack indices, from transposing S and H matrices
				int Jl = i;
				int Il = 2 * k - 2 + j - i;

				ham = -1.0 / (2.0 * Mass) * BSplineObject->BSplineDerivative2OverlapIntegral(i, j);
				overlap = BSplineObject->BSplineOverlapIntegral(i, j);
				
				//Compute potential matrix element if present
				if (HasPotential)
				{
					GetPotentialSlice(potentialSlice, i, PotentialVector);
					ham += BSplineObject->BSplineOverlapIntegral(potentialSlice, i, j);
				}

				//Compute centrifugal matrix element if present
				if (HasCentrifugalPotential)
				{
					GetPotentialSlice(potentialSlice, i, CentrifugalVector);
					CentrifugalMatrix(Ju,Iu) = BSplineObject->BSplineOverlapIntegral(potentialSlice, i, j);
					CentrifugalMatrix(Jl,Il) = BSplineObject->BSplineOverlapIntegral(potentialSlice, i, j);

				}

				PropagationMatrix(Ju, Iu) = overlap + I * dt / 2.0 * ham;
				PropagationMatrix(Jl, Il) = overlap + I * dt / 2.0 * ham;
			}
		}
	}

	else if (PropagationAlgorithm == 1)
	
	{
		//Find local lmin and number of l's
		int lmin = LocalLRange.first();
		int lSize = LocalLRange.length();

		//Resize prop matrix
		BigPropagationMatrix.resize(lSize);
		Pivots.resize(lSize);

		cplx scaling = I * dt / 2.0;

		cplx ham = 0.0;
		cplx overlap = 0.0;

		for (int l_idx = 0; l_idx < lSize; l_idx++)
		{
			//Resize propagation matrix for this subspace
			BigPropagationMatrix(l_idx).resize(N, ldab);
			Pivots(l_idx).resize(N);

			for (int i = 0; i < N; i++)
			{
				int jMax = std::min(i + k, N);
				for (int j = i; j < jMax; j++)
				{
					//Upper lapack indices
					int Ju = j;
					int Iu = 2 * k - 2 + i - j;

					//Lower lapack indices, from transposing S and H matrices
					int Jl = i;
					int Il = 2 * k - 2 + j - i;
					ham = -1.0 / (2.0 * Mass) * BSplineObject->BSplineDerivative2OverlapIntegral(i, j);
					overlap = BSplineObject->BSplineOverlapIntegral(i, j);
					
					//Compute potential matrix element if present
					if (HasPotential)
					{
						GetPotentialSlice(potentialSlice, i, PotentialVector);
						ham += BSplineObject->BSplineOverlapIntegral(potentialSlice, i, j);

					}

					//Compute centrifugal matrix element if present
					if (HasCentrifugalPotential)
					{
						double l = lmin + l_idx;
						cplx factor = l * (l + 1.0) / (2.0 * Mass);
						GetPotentialSlice(potentialSlice, i, CentrifugalVector);
						ham += factor * BSplineObject->BSplineOverlapIntegral(potentialSlice, i, j);
					}

					BigPropagationMatrix(l_idx)(Ju, Iu) = overlap + scaling * ham;
					BigPropagationMatrix(l_idx)(Jl, Il) = overlap + scaling * ham;
				}
			}

			//Initial run to perform LU factorization of subspace propagation matrix
			lapack.CalculateLUFactorizationBanded(BigPropagationMatrix(l_idx) , Pivots(l_idx));
		}

	}

	else if (PropagationAlgorithm == 2)

	{
		//Resize prop matrix and pivots
		BigPropagationMatrix.resize(1);
		BigPropagationMatrix(0).resize(N, ldab);
		Pivots.resize(1);
		Pivots(0).resize(N);

		cplx scaling = I * dt / 2.0;

		cplx ham = 0.0;
		cplx overlap = 0.0;
		for (int i = 0; i < N; i++)
		{
			int jMax = std::min(i + k, N);
			for (int j = i; j < jMax; j++)
			{
				//Upper lapack indices
				int Ju = j;
				int Iu = 2 * k - 2 + i - j;

				//Lower lapack indices, from transposing S and H matrices
				int Jl = i;
				int Il = 2 * k - 2 + j - i;
				ham = -1.0 / (2.0 * Mass) * BSplineObject->BSplineDerivative2OverlapIntegral(i, j);
				overlap = BSplineObject->BSplineOverlapIntegral(i, j);
				
				//Compute potential matrix element if present
				if (HasPotential)
				{
					GetPotentialSlice(potentialSlice, i, PotentialVector);
					ham += BSplineObject->BSplineOverlapIntegral(potentialSlice, i, j);

				}

				BigPropagationMatrix(0)(Ju, Iu) = overlap + scaling * ham;
				BigPropagationMatrix(0)(Jl, Il) = overlap + scaling * ham;
			}

		}
		//Initial run to perform LU factorization of propagation matrix
		lapack.CalculateLUFactorizationBanded(BigPropagationMatrix(0) , Pivots(0));
	}

	else
	
	{
		cout << "ERROR: Unknown propagation algorithm " << PropagationAlgorithm << endl;
	}
}


/*
 * Set up matrices to be stored on BLAS form
 */
template<int Rank>
void Propagator<Rank>::SetupBlasMatrices(const cplx &dt)
{
	cplx Im = cplx(0.0, 1.0);
	int k = BSplineObject->MaxSplineOrder;
	int N = BSplineObject->NumberOfBSplines;
	int lda = k;

	blitz::Array<double, 1> potentialSlice(BSplineObject->GetBSpline(0).extent(0));

	// Resize matrices
	OverlapMatrixBlas.resize(N, lda);
	HamiltonianMatrix.resize(N, lda);
	if (HasCentrifugalPotential) { CentrifugalMatrixBlas.resize(N, lda); }

	for (int j = 0; j < N; j++)
	{
		int iMax = std::min(j + k, N);
		for (int i = j; i < iMax; i++)
		{
			//BLAS index map from "normal" indices
			int J = j;
			int I = i - j;

			//BSplineOverlapIntegral must have j >= i, so we transpose.
			//This is okay, since the matrix is real and symmetric.
			HamiltonianMatrix(J, I)  = -1.0 / (2.0 * Mass) *
				BSplineObject->BSplineDerivative2OverlapIntegral(j, i);
		
			//Compute potential matrix element if present
			if (HasPotential)
			{
				GetPotentialSlice(potentialSlice, j, PotentialVector);
				HamiltonianMatrix(J,I) += BSplineObject->BSplineOverlapIntegral(potentialSlice, j, i);
			}

			//Compute centrifugal matrix element if present
			if (HasCentrifugalPotential)
			{
				GetPotentialSlice(potentialSlice, j, CentrifugalVector);
				CentrifugalMatrixBlas(J,I) = BSplineObject->BSplineOverlapIntegral(potentialSlice, j, i);
			}

			OverlapMatrixBlas(J, I) = BSplineObject->BSplineOverlapIntegral(j, i);
		}
	}
}

/*
 * Get a slice of a function defined on the global grid, starting at
 * the same grid point as b-spline number i.
 */
template<int Rank>
void Propagator<Rank>::GetPotentialSlice(blitz::Array<double, 1> potentialSlice, int i, 
	blitz::Array<double, 1> potentialVector)
{
	int bsplineSize = BSplineObject->GetBSpline(0).extent(0);
	int xSize = BSplineObject->GetNodes().extent(0);
	int globalGridSize = potentialVector.extent(0);

	// Compute start and stop indices of potential (to match b-spline B_i)
	int startIndex = (BSplineObject->GetTopKnotMap()(i) - BSplineObject->MaxSplineOrder + 2) * xSize;
	int stopIndex = std::min(startIndex + bsplineSize, globalGridSize);

	/* 
	 * At degenerate knot points b-splines span a variable number of quadrature points.
	 * To ensure the constant size of the potential slice (as required by BSplineOverlapIntegral),
	 * we make use of a constant-size zero vector, and assign to it the appropriate number of
	 * potential values, possibly leaving some trailing zeros.
	 */
	potentialSlice = 0;
	potentialSlice( blitz::Range(0, stopIndex - startIndex - 1) ) = 
		potentialVector( blitz::Range(startIndex, stopIndex - 1) );

}


//Instantiate propagators of rank 1-4
template class Propagator<1>;
template class Propagator<2>;
template class Propagator<3>;
template class Propagator<4>;

} //Namespace

