

#------------------------------------------------------------------------------------
#                       Interfaces
#------------------------------------------------------------------------------------

class UnsupportedGeometryException(Exception):
	def __init__(self, string):
		Exception.__init__(self, string)

class GeometryInfoBase(object):
	"""
	base for all Geometry information objects
	a GeometryInfo object gives information about how the geometry behaves
	"""

	def UseGridRepresentation(self):
		"""
		Whether the grid representation or the basis representation should
		be used to evaluate the potential
		"""
		raise NotImplementedError()

	def GetGlobalBasisPairCount(self):
		"""
		Returns the number of stored matrix elements
		"""
		raise NotImplementedError()

	def GetLocalBasisPairCount(self):
		return self.GetGlobalBasisPairCount()

	def GetBasisPairs(self):
		"""
		Returns the row-col index pairs for the matrix elements as a 
		two dimensional array. Each row is an index pair, with 
		the first column being the row index, and the second being
		the col-index
		"""
		raise NotImplementedError()

	def GetGlobalBasisPairs(self):
		"""
		returns the row-col index pairs as GetBasisPairs,  only this
		returns the global basis pair list if this rank is 
		distributed
		"""
		if not self.HasParallelMultiply():
			return self.GetBasisPairs()
		else:
			raise NotImplementedError()

	def GetLocalBasisPairIndices(self):
		"""
		Returns the local indices into the array returned by
		GetGlobalBasisPairs() corresponding to the array returned
		by GetBasisPairs(), 
		"""
		if not self.HasParallelMultiply():
			return r_[:self.GetLocalBasisPairCount()]
		else:
			raise NotImplementedError()
	
	def GetStorageId(self):
		"""
		Returns the string defining how the index pairs are ordered
		"""
		raise NotImplementedError()	

	def GetMultiplyArguments(self, psi):
		"""
		Returns the arguments needed for TensorMultiply
		"""
		raise NotImplementedError()

	def HasParallelMultiply(self):
		"""
		Whether this storage supports TensorMultiply when the rank 
		is distributed
		"""
		return False


class GeometryInfoDistributedBase(GeometryInfoBase):
	"""
	Base Geometry information for Geometry infos 
	supporting distributed matrix vector multiply.

	Implementing classes must implement

	SetupBasisPairs(), setting
		self.GlobalIndexPairs		- global basis pairs
		self.LocalIndexPairs        - basis pairs for the current proc
		self.LocalBasisPairIndices  - indices of the local basis pairs in the list of 
		                              global basis pairs

	"""
	def __init__(self):
		self.LocalIndexPairs = None
		self.LocalBasisPairIndices = None
		self.GlobalIndexPairs = None

	def UseGridRepresentation(self):
		return False
	
	def GetGlobalBasisPairCount(self):
		return self.GetGlobalBasisPairs().shape[0] 

	def GetLocalBasisPairCount(self):
		return self.GetBasisPairs().shape[0] 
			
	def GetBasisPairs(self):
		if self.LocalIndexPairs == None:
			self.SetupBasisPairs()
		return self.LocalIndexPairs

	def GetGlobalBasisPairs(self):
		if self.GlobalIndexPairs == None:
			self.SetupBasisPairs()
		return self.GlobalIndexPairs

	def GetLocalBasisPairIndices(self):
		if self.LocalBasisPairIndices == None:
			self.SetupBasisPairs()
		return self.LocalBasisPairIndices	

	def HasParallelMultiply(self):
		return True

	def SetupBasisPairs(self):
		raise NotImplementedError("SetupBasisPairs is not implemented")



class BasisfunctionBase(object):
	"""
	Abstract class which bases all basis function classes.
	Its interface provides 
	"""

	def ApplyConfigSection(self, config):
		raise NotImplementedError()

	def GetGridRepresentation(self):
		"""
		Creates a one dimensional representation for this rank
		in the grid representation
		"""
		raise NotImplementedError()

	def GetBasisRepresentation(self):
		"""
		Creates a one dimensional representation for this rank
		in the basis representation
		"""
		raise NotImplementedError()

	def GetGeometryInfo(self, geometryName):
		raise NotImplementedError()

	def RepresentPotentialInBasis(self, source, dest, rank, geometryInfo, \
			differentiation, configSection):
		raise NotImplementedError()


#------------------------------------------------------------------------------------
#                       Common geometries
#------------------------------------------------------------------------------------

class GeometryInfoCommonDense(GeometryInfoBase):
	"""
	General geometry information for dense matrices
	"""
	def __init__(self, rankCount, useGrid):
		#Set member variables 
		self.RankCount = rankCount
		self.UseGrid = useGrid

	def UseGridRepresentation(self):
		return self.UseGrid
	
	def GetGlobalBasisPairCount(self):
		return self.RankCount**2 
		
	def GetBasisPairs(self):
		pairs = zeros((self.GetGlobalBasisPairCount(), 2), dtype=int32)
		index = 0
		for i in xrange(self.RankCount):
			for j in xrange(self.RankCount):
				pairs[index, 0] = i
				pairs[index, 1] = j
				index+=1

		return pairs

	def GetStorageId(self):
		return "Simp"

	def GetMultiplyArguments(self, psi):
		if not hasattr(self, "BasisPairs"):
			self.BasisPairs = self.GetBasisPairs()
		return [self.BasisPairs]


class GeometryInfoCommonDenseHermitian(GeometryInfoBase):
	"""
	General geometry information for dense matrices
	"""
	def __init__(self, rankCount, useGrid):
		#Set member variables 
		self.RankCount = rankCount	
		self.UseGrid = useGrid

	def UseGridRepresentation(self):
		return self.UseGrid
	
	def GetGlobalBasisPairCount(self):
		N = self.RankCount
		return N * (N + 1) / 2
		
	def GetBasisPairs(self):
		pairs = zeros((self.GetGlobalBasisPairCount(), 2), dtype=int32)
		index = 0
		for i in xrange(self.RankCount):
			for j in xrange(i, self.RankCount):
				pairs[index, 0] = i
				pairs[index, 1] = j
				index+=1

		return pairs

	def GetStorageId(self):
		return "Herm"

	def GetMultiplyArguments(self, psi):
		if not hasattr(self, "BasisPairs"):
			self.BasisPairs = self.GetBasisPairs()
		return [self.BasisPairs]


class GeometryInfoCommonDiagonal(GeometryInfoDistributedBase):
	"""
	General geometry information for diagonal matrices.

	The matrix is diagonal in the sense that all index pairs
	except where row==col, gives a zero contribution. In this case,
	only the row==col elements are stored

	Matrices are usually diagonal when the basis functions are the
	eigenvectors of the potential. They will then also set UseGridRepresentation
	to false. This is the case for the angular momentum
	which is diagonal in the spherical harmonic representation

	V(l, r) =  l*(l+1) / (2 * m * r*r)

	"""
	def __init__(self, repr, useGrid):
		GeometryInfoDistributedBase.__init__(self)
		#Set member variables 
		self.Representation = repr
		self.UseGrid = useGrid

	def UseGridRepresentation(self):
		return self.UseGrid
	
	def SetupBasisPairs(self):
		distr = self.Representation.GetDistributedModel()
		rank = self.Representation.GetBaseRank()
		globalSize = len(self.Representation.GetGlobalGrid(rank))
		localRange = distr.GetLocalIndexRange(globalSize, rank)

		#setup global pairs
		pairs = zeros((globalSize, 2), dtype=int32)
		for i in xrange(globalSize):
			pairs[i, 0] = i
			pairs[i, 1] = i
	
		self.GlobalIndexPairs = pairs
		self.LocalBasisPairIndices = r_[localRange]
		self.LocalIndexPairs = pairs[localRange]

	def GetStorageId(self):
		return "Diag"

	def GetMultiplyArguments(self, psi):
		return []

		#return False


class GeometryInfoCommonBandedDistributed(GeometryInfoDistributedBase):
	"""
	Geometry for a banded matrix with support for parallel
	TensorMultiply
	"""
	def __init__(self, repr, bandCount, useGrid):
		GeometryInfoDistributedBase.__init__(self)
		#Set member variables 
		self.BandCount = bandCount
		self.UseGrid = useGrid
#		assert(repr.GetBaseRank() == 0)
#		self.BaseRank = 0 #Supports only distributed in the first rank
		assert(repr.GetBaseRank() in [0,1])
		self.BaseRank = repr.GetBaseRank()
		self.Representation = repr
		self.RankCount = int(repr.GetFullShape()[repr.GetBaseRank()])
		self.TempArrays = None
		self.MultiplyArguments = None

	def UseGridRepresentation(self):
		return self.UseGrid
	
	def SetupBasisPairs(self):
		distr = self.Representation.GetDistributedModel()
		rank = self.Representation.GetBaseRank()
		globalBasisPairCount = self.RankCount * (2 * self.BandCount + 1)
		localRange = distr.GetLocalIndexRange(int(globalBasisPairCount), rank)

		#setup global pairs
		pairs = zeros((globalBasisPairCount, 2), dtype=int32)
		index = 0
		for i in xrange(self.RankCount):
			for j in xrange(i-self.BandCount, i+self.BandCount+1):
				if 0 <= j < self.RankCount:
					packedRow = j
					packedCol = self.BandCount - j + i

					index = packedRow * (2 * self.BandCount + 1) + packedCol
					pairs[index, 0] = i
					pairs[index, 1] = j

		#store pairs
		self.GlobalIndexPairs = pairs
		self.LocalBasisPairIndices = r_[localRange]
		self.LocalIndexPairs = pairs[localRange]

	def GetStorageId(self):
		return "Distr"

	def GetMultiplyArguments(self, psi):
		if self.MultiplyArguments == None:
			if self.TempArrays == None:
				self.SetupTempArrays(psi)
			self.MultiplyArguments = [self.RankCount, self.BandCount] + self.TempArrays

		return self.MultiplyArguments

	def SetupTempArrays(self, psi):
		dataShape = psi.GetData().shape
		rank = self.BaseRank

		recvTempShape = list(dataShape)
		recvTempShape[rank] = 1
		recvTemp = zeros(recvTempShape, dtype=complex)

		sendTempShape = list(dataShape)
		sendTempShape[rank] = 2
		sendTemp = zeros(sendTempShape, dtype=complex)

		self.TempArrays = [recvTemp, sendTemp]



class GeometryInfoCommonIdentity(GeometryInfoBase):
	"""
	General geometry information for Identity matrices. 

	By Identity in this setting, we mean that it is independent
	of the variable in this rank, i.e it will be Identity in 
	the y rank in the following example

	V(x,y) = f(x)

	For Identity to work properly, the basis should be orthogonal. 
	In this case, the above function will give 
				{  f(x) if i'==i
	V(x,i'i) =  {
				{  0 if i'!=i
	if the basis is not orthogonal, we will get the overlap matrix
	S_{i',i} if i!=i, which will ruin this scheme

	To make less special cases, this class will return 1 
	index pair, (0, 0)
	"""
	def __init__(self, useGrid):
		#Set member variables 
		self.UseGrid = useGrid

	def UseGridRepresentation(self):
		return self.UseGrid
	
	def GetGlobalBasisPairCount(self):
		return 1 
		
	def GetBasisPairs(self):
		pairs = zeros((1, 2), dtype=int32)
		pairs[0, 0] = 0
		pairs[0, 1] = 0

		return pairs

	def GetStorageId(self):
		return "Ident"

	def GetMultiplyArguments(self, psi):
		return []


class GeometryInfoCommonBandedNonHermitian(GeometryInfoBase):
	"""
	Geometry information for general banded matrices, the potential
	is stored in the BLAS general banded format such that 
	multiplication can be done with blas calls
	"""
	def __init__(self, rankCount, bandCount, useGrid):
		"""
		rankCount is the number of rows in the matrix
		bandCount is the number of super diagonals.

		The matrix is assumed to have equally many super 
		and sub diagonals, making the total number of diagonals
		2 * bandCount + 1
		"""
		#Set member variables 
		self.RankCount = rankCount
		self.BandCount = bandCount
		self.UseGrid = useGrid

	def UseGridRepresentation(self):
		return self.UseGrid
	
	def GetGlobalBasisPairCount(self):
		return self.RankCount * (self.BandCount * 2 + 1) 

	def GetBasisPairs(self):
		count = self.GetGlobalBasisPairCount()

		N = self.RankCount
		k = self.BandCount

		pairs = zeros((self.GetGlobalBasisPairCount(), 2), dtype=int32)
		pairs[:,0] = 0
		pairs[:,1] = N-1

		index = 0
		for i in xrange(N):
			for j in xrange(i-k, i+k+1):
				if 0 <= j < N:
					blasJ = j
					blasI = k + i - j

					index = blasJ * (2*k+1) + blasI
					pairs[index, 0] = i
					pairs[index, 1] = j

		return pairs
	
	def GetStorageId(self):
		return "BandNH"

	def GetMultiplyArguments(self, psi):
		return []




