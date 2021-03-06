#include "distributedoverlapmatrix.h"
#include "../utility/blitztricks.h"
#include <mpi.h>

#include <Epetra_LinearProblem.h>

#include <Amesos_ConfigDefs.h>
#include <Amesos.h>
//#include <Amesos_Superludist.h>

#include <Teuchos_RCP.hpp>

using namespace Teuchos;
using namespace blitz;

template<int Rank>
void DistributedOverlapMatrix<Rank>::SetupRank(Wavefunction<Rank> &srcPsi, int opRank)
{
	//Need a copy of srcPsi for future reference (only on first call to this function)
	if (!HasPsi)
	{
		Psi = srcPsi.Copy();
		HasPsi = true;
	}

	//Check that distribution for opRank has not changed since last call.
	//Also check that typeID of representation is the same
/*	int curDistribOpRank = Psi->GetRepresentation()->GetDistributedModel()->GetDistribution()(opRank);
	int srcDistribOpRank = srcPsi.GetRepresentation()->GetDistributedModel()->GetDistribution()(opRank);
	if ( (curDistribOpRank != srcDistribOpRank) )
	{
		Psi = srcPsi.Copy();

		//NB: We reset IsSetup flag for _all_ ranks!
		IsSetupRank = false;
	}
*/
	if (!IsSetupRank(opRank))
	{
		//Sanity check: operation rank should be less than rank of wavefunction (and nonzero, duh)
		assert(opRank < Rank);
		assert(opRank > -1);

		//Assert non-orthogonal rank opRank
		assert (!srcPsi.GetRepresentation()->IsOrthogonalBasis(opRank));

		//Create Epetra map for this rank
		WavefunctionMaps(opRank) = CreateWavefunctionMultiVectorEpetraMap<Rank>(Psi, opRank);

		//Setup overlap matrix
		SetupOverlapMatrixRank(srcPsi, opRank);

		//Setup work multivectors
		blitz::Array<cplx, 3> psiData = MapToRank3(srcPsi.GetData(), opRank, 1);
		int numVectors = 2 * psiData.extent(0) * psiData.extent(2);
		InputVector(opRank) = Epetra_MultiVector_Ptr( new Epetra_MultiVector(*WavefunctionMaps(opRank), numVectors, false) );
		OutputVector(opRank) = Epetra_MultiVector_Ptr( new Epetra_MultiVector(*WavefunctionMaps(opRank), numVectors, false) );

		//Allocate mem for multivectors
		//InputData.resize(srcPsi.GetData().size(), 1);
		//OutputData.resize(srcPsi.GetData().size(), 1);
	
		//Setup Amesos solvers
		SetupOverlapSolvers(srcPsi, opRank);
		
		//Flag this rank as set up
		IsSetupRank(opRank) = true;
	}
}


template<int Rank>
void DistributedOverlapMatrix<Rank>::SetupOverlapMatrixRank(Wavefunction<Rank> &srcPsi, int opRank)
{
	//Get overlap matrix for opRank (and full col data)
	OverlapMatrix::Ptr overlap = srcPsi.GetRepresentation()->GetGlobalOverlapMatrix(opRank);
	blitz::Array<double, 2> overlapFullCol = overlap->GetOverlapFullCol();

	//Overlap row size
	int numSuperDiagonals = overlap->GetSuperDiagonals();
	int colSizeFull = overlap->GetBasisSize();

	//Epetra CrsMatrix for opRank overlap
	//cout << "Allocating Epetra CrsMatrix " << numSuperDiagonals << endl;
	//int numTotalBands = 2 * numSuperDiagonals - 1;
	//Epetra_CrsMatrix_Ptr overlapMatrix = Epetra_CrsMatrix_Ptr( new Epetra_CrsMatrix(Copy, *wavefunctionMap, numTotalBands, true) );

	//Calculate the number of elements per (proc local) row
	int globalStartRow = WavefunctionMaps(opRank)->MinMyGID();
	int globalEndRow = WavefunctionMaps(opRank)->MaxMyGID();
	int numRowsProc = globalEndRow - globalStartRow + 1;
	blitz::Array<int, 1> RowLengths;
	RowLengths.resize(numRowsProc);
	for (int i=globalStartRow; i<=globalEndRow; i++)
	{
		int startCol = std::max(i - numSuperDiagonals, 0);
		int endCol = std::min(i + numSuperDiagonals + 1, colSizeFull);
		RowLengths(i - globalStartRow) = endCol - startCol + 1;
	}

	OverlapMatrices(opRank) = Epetra_CrsMatrix_Ptr( new Epetra_CrsMatrix(Copy, *WavefunctionMaps(opRank), RowLengths.data(), true) );

	//Debug output from Epetra
	OverlapMatrices(opRank)->SetTracebackMode(4);

	/*int procId = 0;
	int procCount = 0;
	MPI_Comm_rank(MPI_COMM_WORLD, &procId);
	MPI_Comm_size(MPI_COMM_WORLD, &procCount);
	for (int i=0; i<procCount; i++)
	{
		if (procId == i)
		{
			cout << "Processor " << i << endl;
			cout << "Wavefunction Map " << *WavefunctionMaps(opRank) << endl;
			cout << endl;
		}
	}*/

	//Copy overlap slice corresponding to this proc into CrsMatrix. Since we have the entire overlap
	//matrix (for opRank) on every proc, global and local indices are the same. We only have to
	//determine start and end of row slice for this proc, and the first column index (banded case).
	for (int i=globalStartRow; i<=globalEndRow; i++)
	{
		int startCol = std::max(i - numSuperDiagonals, 0);
		int endCol = std::min(i + numSuperDiagonals + 1, colSizeFull);
		for (int j=startCol; j<endCol; j++)
		{
			int bandIdx = j - i + numSuperDiagonals;

			OverlapMatrices(opRank)->InsertGlobalValues(i, 1, &overlapFullCol(i, bandIdx), &j);
		}
	}

	//Signal end of matrix input
	OverlapMatrices(opRank)->FillComplete();
}

/*
 * Set up Amesos solver for each rank in wavefunction. The choice of solver type is determined
 * by whether the wavefunction rank is distributed or not (SuperLU_dist or KLU).
 */
template<int Rank>
void DistributedOverlapMatrix<Rank>::SetupOverlapSolvers(Wavefunction<Rank> &psi, int opRank)
{
	//Set up Epetra LinearProblem with overlap for this rank and input/output multivectors
	EpetraProblems(opRank) = Epetra_LinearProblem_Ptr( new Epetra_LinearProblem(OverlapMatrices(opRank).get(), OutputVector(opRank).get(), InputVector(opRank).get()) );

	//Determine solver type. Use SuperLU_dist if opRank is distributed, else KLU
	Amesos Factory;
	std::string SolverType;
	if (psi.GetRepresentation()->GetDistributedModel()->IsDistributedRank(opRank))
	{
		SolverType = "Amesos_Superludist";
	}
	else
	{
		SolverType = "Amesos_Klu";
	}

	//Create Amesos solver for this rank
	Solvers(opRank) = Amesos_BaseSolver_Ptr( Factory.Create(SolverType, *EpetraProblems(opRank)) );

	//Check that requested solver exists in Amesos build
	if (Solvers(opRank) == 0)
	{
		throw std::runtime_error("Specified Amesos solver not available");
	}

	//Setup the parameter list for the solver (TODO: get these from Python config section)
	Teuchos::ParameterList List;
	List.set("MatrixType", "symmetric");
	List.set("PrintTiming", false);
	List.set("PrintStatus", false);
	List.set("ComputeTrueResidual", false);
	Solvers(opRank)->SetParameters(List);

	//Perform symbolic factorization
	Solvers(opRank)->SymbolicFactorization();
	Solvers(opRank)->NumericFactorization();
}


template<int Rank>
shared_ptr<Epetra_MultiVector> DistributedOverlapMatrix<Rank>::SetupMultivector(Wavefunction<Rank> &srcPsi, int opRank)
{
	//Setup Epetra stuff for opRank
	SetupRank(srcPsi, opRank);

	//Map wavefunction to 3D array (compress before- and after-ranks)
	blitz::Array<cplx, 3> psiData = MapToRank3(srcPsi.GetData(), opRank, 1);
	int beforeSize = psiData.extent(0);
	int opSize = psiData.extent(1);
	int afterSize = psiData.extent(2);
	int otherSize = beforeSize*afterSize;

	//Copy real and imag part of wavefunction into 2D array
	blitz::Array<double, 2> data;
	data.resize(2 * otherSize, opSize);
	for (int i=0; i<beforeSize; i++)
	{
		for (int j=0; j<opSize; j++)
		{
			for (int k=0; k<afterSize; k++)
			{
				data(i*afterSize + k, j) = real( psiData(i,j,k) );
				data(i*afterSize + k + otherSize, j) = imag( psiData(i,j,k) );
			}
		}
	}

	//Number of vectors (in multivector)
	int numVectors = 2 * otherSize; 

	//Create Epetra multivector (view of data)
	Epetra_MultiVector_Ptr srcVec = Epetra_MultiVector_Ptr(new Epetra_MultiVector(Copy, *WavefunctionMaps(opRank), data.data(), opSize, numVectors));

	return srcVec;
}


/*
 * WavefunctionToMultiVector
 */
template<int Rank>
void DistributedOverlapMatrix<Rank>::WavefunctionToMultiVector(Wavefunction<Rank> &psi, Epetra_MultiVector_Ptr vec, int opRank)
{
	//Map wavefunction to 3D array (compress before- and after-ranks)
	blitz::Array<cplx, 3> psiData = MapToRank3(psi.GetData(), opRank, 1);
	int beforeSize = psiData.extent(0);
	int opSize = psiData.extent(1);
	int afterSize = psiData.extent(2);
	int otherSize = beforeSize*afterSize;

	//Copy real and imag part of wavefunction into multivector
	double realVal, imagVal;
	for (int i=0; i<beforeSize; i++)
	{
		for (int j=0; j<opSize; j++)
		{
			for (int k=0; k<afterSize; k++)
			{
				//cout << "cur vec = " << i * afterSize + k << ", cur idx = " << j << endl;
				//cout << "cur vec = " << i * afterSize + k + otherSize << ", cur idx = " << j << endl;
				
				realVal = real( psiData(i,j,k) );
				imagVal = imag( psiData(i,j,k) );
				vec->ReplaceMyValue(j, i*afterSize + k, realVal);
				vec->ReplaceMyValue(j, i*afterSize + k + otherSize, imagVal);
			}
		}
	}
}


/*
 * MultiVectorToWavefunction
 */
template<int Rank>
void DistributedOverlapMatrix<Rank>::MultiVectorToWavefunction(Wavefunction<Rank> &psi, Epetra_MultiVector_Ptr vec, int opRank)
{
	//Put result into psi
	blitz::Array<cplx, 3> data = MapToRank3(psi.GetData(), opRank, 1);
	int beforeSize = data.extent(0);
	int opSize = data.extent(1);
	int afterSize = data.extent(2);
	int otherSize = beforeSize*afterSize;
	double *destVecView=0;
	vec->ExtractView(&destVecView, &opSize);
	blitz::Array<double, 2> data2(destVecView, shape(2 * otherSize, opSize), shape(opSize, 1), neverDeleteData);
	//blitz::Array<double, 2> data2;
	//data2.resize(2 * otherSize, opSize);
	//vec->ExtractCopy(data2.data(), opSize);
	for (int i=0; i<beforeSize; i++)
	{
		for (int j=0; j<opSize; j++)
		{
			for (int k=0; k<afterSize; k++)
			{
				double realVal = data2(i*afterSize + k, j);
				double imagVal = data2(i*afterSize + k + otherSize, j);
				data(i,j,k) = realVal + I * imagVal;
			}
		}
	}
}


template<int Rank>
void DistributedOverlapMatrix<Rank>::MultiplyOverlapRank(Wavefunction<Rank> &srcPsi, Wavefunction<Rank> &destPsi, int opRank, bool fastAlgo)
//void DistributedOverlapMatrix<Rank>::MultiplyOverlapRank(Wavefunction<Rank> &psi, int opRank, bool fastAlgo)
{
	SetupRank(srcPsi, opRank);

	if (fastAlgo)
	{
		//Setup source multivector
		WavefunctionToMultiVector(srcPsi, InputVector(opRank), opRank);

		//Perform matrix-vector multiplication
		OverlapMatrices(opRank)->Multiply(false, *InputVector(opRank), *OutputVector(opRank));

		//Put result multivector into destination wavefunction
		MultiVectorToWavefunction(destPsi, OutputVector(opRank), opRank);
	}
	else
	{

		//Setup source multivector
		Epetra_MultiVector_Ptr srcVec = SetupMultivector(srcPsi, opRank);
	
		//Output multivector (copy of input)
		Epetra_MultiVector_Ptr destVec = Epetra_MultiVector_Ptr(new Epetra_MultiVector(*srcVec));

		//Perform matrix-vector multiplication
		OverlapMatrices(opRank)->Multiply(false, *srcVec, *destVec);

		//Put result multivector into destination wavefunction
		MultiVectorToWavefunction(destPsi, destVec, opRank);
	}
}


template<int Rank>
void DistributedOverlapMatrix<Rank>::SolveOverlapRank(Wavefunction<Rank> &srcPsi, Wavefunction<Rank> &destPsi, int opRank, bool fastAlgo)
//void DistributedOverlapMatrix<Rank>::SolveOverlapRank(Wavefunction<Rank> &psi, int opRank, bool fastAlgo)
{
	SetupRank(srcPsi, opRank);
	
	if (fastAlgo)
	{
		//Setup source multivector
		WavefunctionToMultiVector(srcPsi, InputVector(opRank), opRank);
		
		//Solve
		AMESOS_CHK_ERRV(Solvers(opRank)->Solve());

		//Put solution multivector into destination wavefunction
		MultiVectorToWavefunction(destPsi, OutputVector(opRank), opRank);
	}
	else
	{
		//Setup source multivector
		Epetra_MultiVector_Ptr srcVec =  SetupMultivector(srcPsi, opRank);

		//Output multivector (copy of input)
		Epetra_MultiVector_Ptr destVec = Epetra_MultiVector_Ptr(new Epetra_MultiVector(*srcVec));

		//Set up Epetra LinearProblem with overlap for this rank and input/output multivectors
		Epetra_LinearProblem Problem(OverlapMatrices(opRank).get(), destVec.get(), srcVec.get());

		//Initialize Amesos solver and factory
		Amesos_BaseSolver* Solver;
		Amesos Factory;
		std::string SolverType = "Amesos_Superludist";
		Solver = Factory.Create(SolverType, Problem);

		//Check that requested solver exists in Amesos
		if (Solver == 0)
		{
			throw std::runtime_error("Specified Amesos solver not available");
		}

		//Setup the parameter list for the solver
		Teuchos::ParameterList List;
		//List.set("PrintTiming", true);
		//List.set("PrintStatus", true);
		Solver->SetParameters(List);

		//Factorization and solve
		Solver->SymbolicFactorization();
		Solver->NumericFactorization();
		AMESOS_CHK_ERRV(Solver->Solve());

		//Put solution multivector into destination wavefunction
		MultiVectorToWavefunction(destPsi, destVec, opRank);

		//Solver->PrintTiming();
	}

}

template class DistributedOverlapMatrix<1>;
template class DistributedOverlapMatrix<2>;
template class DistributedOverlapMatrix<3>;
template class DistributedOverlapMatrix<4>;

