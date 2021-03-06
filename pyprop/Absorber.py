

"""

Example config.ini sections

[AbsorbingPotential]
type = PotentialType.Dynamic
classname = "CombinedAbsorber"
absorbers = ["RadialAbsorber"]

[RadialAbsorber]
type = AbsorbingBoundary
rank = 0
width = 10
absorb_left = True
absorb_right = True

"""


class AbsorbingBoundary(core.AbsorberModel):

	def ApplyConfigSection(self, config):
		self.Width = config.width
		self.AbsorbLeft = config.absorb_left
		self.AbsorbRight = config.absorb_right

		self.AbsorbFactor = 1.0
		if hasattr(config, "absorb_factor"):
			self.AbsorbFactor = config.absorb_factor

	def SetupStep(self, grid, globalGrid):
		#minPos = min(grid)
		#maxPos = max(grid)
		minPos = min(globalGrid)
		maxPos = max(globalGrid)

		self.Scaling = ones(len(grid), dtype=double)
		for i in range(len(grid)):	
			pos = grid[i]
			width = self.Width
			scaling = 1
			if pos < minPos + width and self.AbsorbLeft:
				scaling *= cos(pi/2.0 * (pos - (minPos + width)) / width)**(1./8.);
			if pos > maxPos - width and self.AbsorbRight:
				scaling *= cos(pi/2.0 * (- maxPos + pos + width) / width)**(1./8.);
			self.Scaling[i] *= (scaling * self.AbsorbFactor) + 1 - self.AbsorbFactor

	def	GetScaling(self):
		return self.Scaling


class CombinedAbsorber:
	"""
	Combined absorber is an absorber that can contain a number of sub-absorbers,
	where each perform in a specific direction.

	"""
	
	def ApplyConfigSection(self, configSection):
		config = configSection.Config
		self.AbsorberList = []
		absorberList = configSection.absorbers

		self.SetupComplete = False

		for absorberName in absorberList:
			#Create absorber
			absorberConfig = config.GetSection(absorberName)
			absorber = absorberConfig.type()
			#Apply configsection
			absorberConfig.Apply(absorber)
			#Add to absorberlist with rank
			self.AbsorberList.append( (absorber, absorberConfig.rank) )
			
	def SetupStep(self, psi, dt):
		rank = psi.GetRank()
		self.AbsorberPotential = CreateInstanceRank("core.CombinedAbsorberPotential", rank)

		repr = psi.GetRepresentation()
		for absorber, rank in self.AbsorberList:

			#If the wavefunction is distributed along the absorber rank(s), 
			#we should only apply absorber on the procs which have the 
			#parts of the grid where the absorber is supposed to be active.
			#For the moment, absorber cannot extend across multiple procs.
#			distrModel = repr.GetDistributedModel()
#			if distrModel.IsDistributedRank(rank):
#				if ProcId == 0:
#					absorber.AbsorbRight = False
#					if absorber.Width > repr.GetLocalGrid(rank).size:
#						print "WARNING: absorber across multiple procs not currently supported"
#				if ProcId == ProcCount - 1:
#					if absorber.Width > repr.GetLocalGrid(rank).size:
#						print "WARNING: absorber across multiple procs not currently supported"
#					absorber.AbsorbLeft = False
#				else:
#					absorber.Width = 0
#					absorber.AbsorbLeft = False
#					absorber.AbsorbRight = False

			localGrid = repr.GetLocalGrid(rank)
			globalGrid = repr.GetGlobalGrid(rank)
			absorber.SetupStep(localGrid, globalGrid)
			self.AbsorberPotential.AddAbsorber(absorber, rank)

			self.SetupComplete = True


	def MultiplyPotential(self, psi, destPsi, dt, t):
		"""
		MultiplyPotential for absorber does not do anything.
		"""
		pass
	
	def CalculateExpectationValue(self, psi, dt, t):
		return 0

	def ApplyPotential(self, psi, dt, t):
		if not self.SetupComplete:
			self.SetupStep(psi, dt)
		self.AbsorberPotential.ApplyPotential(psi, dt, t)
		
