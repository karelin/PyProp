
#------------------------------------------------------------------------------------
#                       Spectrum Finder
#------------------------------------------------------------------------------------

def FindSpectrumEigenvalues(shift, **args):
	"""
	Finds eigenvalues near a shift for the SpectrumFinder class.

	After completion, it saves the eigenvalues to a messageFile, and the
	eigenvectors to an eigenvectorFile
	"""
	#Setup output files
	messageFile = args["messageFile"]
	eigenvectorFile = args["eigenvectorFile"]
	pyprop.PrintOut("Message File = %s" % messageFile)

	#Perform Calculation
	solver, shiftInvertSolver, eigenvalues = FindEigenvaluesNearShift(shift, **args)

	#Print Statistics
	if pyprop.ProcId == 0:
		shiftInvertSolver.PrintStatistics()
	PrintOut("SHIFT = %s" % prop.Config.GMRES.shift)
	PrintOut("GMRESERRORS = %s" % shiftInvertSolver.Solver.GetErrorEstimateList())
	PrintOut("EIGENVALUES = %s" % eigenvalues)

	#save eigenvectors
	SaveEigenvalueSolverShiftInvert(eigenvectorFile, solver, shiftInvertSolver, shift)

	#Save message file
	sortIdx = argsort(real(eigenvalues))
	eigenvalues = eigenvalues[sortIdx]
	if pyprop.ProcId == 0:
		f = tables.openFile(messageFile, "w")
		f.createArray(f.root, "eigenvalues", eigenvalues)
		f.close()


class SpectrumFinder(object):
	"""
	Class for finding all eigenvalues/eigenvectors in a 
	range of eigenvalues. This is done by running a series
	of FindSpectrumEigenvalues. Each call returns a number
	of eigenvalues near a given shift. The intervals between 
	startEigenvalue and endEigenvalue are subdivided until 
	the eigenvalues from two nearby shifts overlap. 

	This algorithm has one serious flaw: we might not find all
	eigenvalues, if the edge-eiggenvalues between two shifts are
	degenerate.

	Another inefficiency is that as we do not try to guess the
	density of states, the final subdivision may cause the eigenvalues
	of two nearby shifts to overlap severly.
	
	"""

	class Message(object):
		def __init__(self, shift, interval, messageId, startTime, msgFolder, dataFolder):
			self.MessagesFolder = msgFolder
			self.DataFolder = dataFolder
			self.Shift = shift
			self.MessageId = messageId
			self.StartTime = startTime
			self.Interval = interval

		def GetMessageFile(self):
			return os.path.join(self.MessagesFolder, "message_%s.h5" % (self.MessageId))
			
		def GetDataFile(self):
			return os.path.join(self.DataFolder, "eigenvectors_%s.h5" % (self.MessageId))
		
	def __init__(self, startEigenvalue, endEigenvalue, folderPrefix, **args):
		self.StartEigenvalue = startEigenvalue
		self.EndEigenvalue = endEigenvalue
		self.Arguments = args
		self.DegeneracyTolerance = 1e-9

		#Setup folders
		self.MessagesFolder = os.path.join(folderPrefix, "messages")
		self.DataFolder = os.path.join(folderPrefix, "data")
		self.StatusFolder = folderPrefix
		if not os.path.exists(self.MessagesFolder):
			os.makedirs(self.MessagesFolder)
		if not os.path.exists(self.DataFolder):
			os.makedirs(self.DataFolder)
		if not os.path.exists(self.StatusFolder):
			os.makedirs(self.StatusFolder)

		procCount = args.get("procCount", None)
		#if no proccount is specified, use one for each angular basis function
		if procCount == None:
			conf = SetupConfig(**args)
			lcount = len([1 for l in conf.AngularRepresentation.index_iterator])
			self.ProcCount = lcount
		else:
			self.ProcCount = procCount
		self.ProcPerNode = 4

		#Setup maps to keep track of which eigenvalues we have found
		self.NextMessageId = 0
		self.ActiveMessages = []  #Messages we are waiting for to arrive
		self.CompletedMessages = []     #List over completed messages 
		self.ShiftMap = {}		  #Maps shifts to a list of eigenvalues
		self.ActiveIntervals = []
		self.ErrorMessages = []
	
	def GetNextMessageId(self):
		msg = self.NextMessageId
		self.NextMessageId += 1
		return msg

	def StartMessage(self, shift, interval, messageId):
		#Create message
		message = self.Message(shift, interval, messageId, time.time(), self.MessagesFolder, self.DataFolder)
		messageFile = message.GetMessageFile()
		eigenvectorFile = message.GetDataFile()

		#Remove files if it exists
		if os.path.exists(messageFile):
			os.remove(messageFile)
		if os.path.exists(eigenvectorFile):
			os.remove(eigenvectorFile)

		#Start job
		jobId = RunSubmit(FindSpectrumEigenvalues, self.ProcCount, self.ProcPerNode, shift=shift, \
			messageFile=messageFile, eigenvectorFile=eigenvectorFile, **self.Arguments)
		message.JobId = jobId
		self.ActiveMessages.append(message)

	def WaitForCompletedMessages(self):
		timeout = False
		pyprop.PrintOut("Waiting for %i messages to complete" % len(self.ActiveMessages))
		while len(self.ActiveMessages) > 0 and not timeout:
			completedMessages = []
			errorMessages = []

			#check for finished messages
			for msg in list(self.ActiveMessages):
				if submitpbs.CheckJobCompleted(msg.JobId):
					msgPath = os.path.abspath(msg.GetMessageFile())
					if os.path.exists(msgPath):
						completedMessages.append(msg)
					else:
						print "Job Status for error message: "
						print submitpbs.GetJobStatus(msg.JobId)
						self.ErrorMessages.append(msg)
						self.ActiveMessages.remove(msg)

			#process completed messages
			map(self.MessageCompleted, completedMessages)
		
			if len(completedMessages) == 0:
				time.sleep(10)

	def MessageCompleted(self, msg):
		#get eigenvalues
		f = tables.openFile(msg.GetMessageFile())
		try:
			eigenvalues = f.root.eigenvalues[:]
		finally:
			f.close()

		#Update message lists
		self.ActiveMessages.remove(msg)
		self.CompletedMessages.append(msg)

		pyprop.PrintOut("Found %i eigenvalues around %f" % (len(eigenvalues), msg.Shift))
		self.ShiftMap[msg.Shift] = eigenvalues

		#Check if this is one of the first two eigenvalues
		if msg.Interval == None:
			#if this is message the last of the boundary messages
			if len(self.ShiftMap) == 2:
				self.ActiveIntervals.append((self.StartEigenvalue, self.EndEigenvalue))

		else:
			#Otherwise subdivide the interval and mark it active
			start, end = msg.Interval
			shift = msg.Shift
			self.ActiveIntervals.append((start, shift))
			self.ActiveIntervals.append((shift, end))

	def RunIteration(self):
		#if this is the first iteration:
		if len(self.ShiftMap) == 0:
			self.StartMessage(self.StartEigenvalue, None, self.GetNextMessageId())
			self.StartMessage(self.EndEigenvalue, None, self.GetNextMessageId())

		for start, end in self.ActiveIntervals:
			evStart = self.ShiftMap[start]
			evEnd = self.ShiftMap[end]

			#Check whether there can be a gab between the last eigenvalue of start 
			#and the smallest eigenvalue of end
			if evStart[-1] < evEnd[0]:
				shift = (end + start) / 2.
				print "Starting message at shift %s" % shift
				self.StartMessage(shift, (start, end), self.GetNextMessageId())
		self.ActiveIntervals = []

		self.WaitForCompletedMessages()
	
	def FilterActiveIntervals(self):
		def intervalNeedsSubdivision(interval):
			start, end = interval
			evStart = self.ShiftMap[start]
			evEnd = self.ShiftMap[end]
		
			#Check whether there can be a gab between the last eigenvalue of start 
			#and the smallest eigenvalue of end
			return evStart[-1] < evEnd[0]

		return filter(intervalNeedsSubdivision, self.ActiveIntervals)

	def RunToEnd(self):
		self.RunIteration()
		while len(self.ActiveIntervals) > 0:
			self.RunIteration()

	def LoadAvailableMessages(self):
		dataFiles = os.listdir(self.DataFolder)
		dataFiles = filter(lambda x: x.startswith("eigenvectors_"), dataFiles)
		dataFiles = map(lambda x: os.path.join(self.DataFolder, x), dataFiles)

		selfConf = SetupConfig(**self.Arguments)

		def getMessage(dataFile):
			messageId = int(os.path.splitext(dataFile)[0].split("_")[1])
			startTime = os.path.getctime(dataFile)
			f = tables.openFile(dataFile, "r")
			try:
				conf = pyprop.Config(f.root.Eig._v_attrs.configObject)
				if not CheckCompatibleRadialGrid(selfConf, conf):
					print "Eigenvalues from file %s are from incompatible grid" % dataFile
					return None

				shift = float(conf.GMRES.shift)
				eigenvalues = sorted(f.root.Eig.Eigenvalues[:])
			except:
				print "Could not load eigenvalues and shift from file %s" % dataFile
				return None
			finally:
				f.close()

			message = self.Message(shift, (-Inf, Inf), messageId, startTime, self.MessagesFolder, self.DataFolder)
			return message, eigenvalues

		#Get messages and eigenvalues from datafiles
		first = lambda x: x[0]
		notNone = lambda x: not x == None
		messageList, eigenvalueList = zip(*filter(notNone, map(getMessage, dataFiles)))

		#Setup Completed Messages list
		self.CompletedMessages += messageList
		#Setup ShiftMap
		for msg, ev in zip(messageList, eigenvalueList):
			self.ShiftMap[msg.Shift] = ev
		#Setup ActiveIntervals
		shiftList = sorted(self.ShiftMap.keys())
		self.ActiveIntervals += [(start, end) for start, end in zip(shiftList[0:-1], shiftList[1:])]
		#next message id
		self.NextMessageId = 1 + max(map(lambda x: x.MessageId, self.CompletedMessages))

	def CancelMessage(self, message):
		pyprop.PrintOut("Canceling message for shift = %s" % message.Shift)
		if message not in self.ActiveMessages:
			raise Exception("Message not active")
		#Put the interval back into active intervals
		self.ActiveIntervals.append(message.Interval)
		#Remove message from active messages
		self.ActiveMessages.remove(message)

	def CancelAllMessages(self):
		for msg in list(self.ActiveMessages):
			self.CancelMessage(msg)
	
	def GetSortedEigenvalues(self):
		eigenvalueList = []
		overlappingEigenvalues = 0
		for shift in sorted(self.ShiftMap.keys()):
			for curIndex, curE in enumerate(self.ShiftMap[shift]):
				if len(eigenvalueList) == 0 or curE > eigenvalueList[-1] + self.DegeneracyTolerance:
					if len(eigenvalueList) > 1 and curIndex==0:
						gap = curE - eigenvalueList[-1]
						avgDeltaE = average(diff(eigenvalueList[-10:]))
						print "Possible gap found between eigenvalues %f and %f. Estimated %i missing eigenvalues." % (eigenvalueList[-1], curE, gap/avgDeltaE)
					eigenvalueList.append(curE)
				else:
					overlappingEigenvalues += 1
	
		print "Eiganvalues found       = %i" % (len(eigenvalueList))
		print "Overlapping eigenvalues = %i" % (overlappingEigenvalues)
	
		return eigenvalueList

	def GetSortedEigenvectors(self):
		eigenvalues = self.GetSortedEigenvalues()
		eigenvectorFile = os.path.join(self.StatusFolder, "eigenvectors.h5")

		shiftToMessage = dict([(msg.Shift, msg) for msg in self.CompletedMessages])

		f = tables.openFile(eigenvectorFile, "w")
		try:
			eigGroup = f.createGroup(f.root, "Eig")
			f.createArray(eigGroup, "Eigenvalues", eigenvalues)
		
			#Store metadata from this spetrum finder
			attrs = eigGroup._v_attrs
			attrs.StartEigenvalue = self.StartEigenvalue
			attrs.EndEigenvalue = self.EndEigenvalue
			attrs.Arguments = self.Arguments
			attrs.FolderPrefix = self.StatusFolder
		
			index = 0
			eigenvalueList = []
			for shift in sorted(self.ShiftMap.keys()):
				msg = shiftToMessage[shift]
				curFile = tables.openFile(msg.GetDataFile(), "r")
				try:
					curEigenvalues = curFile.root.Eig.Eigenvalues[:]
					eigIndex = argsort(curEigenvalues)
					for curIndex, curE in zip(eigIndex, curEigenvalues[eigIndex]):
						if index == 0 or curE > eigenvalueList[-1] + self.DegeneracyTolerance:
							infoStr =  "Progress: %3i%%" % ((index * 100)/ len(eigenvalues))
							sys.stdout.write("\b"*15 + infoStr)
							sys.stdout.flush()

							curNode = curFile.getNode(GetEigenvectorDatasetPath(curIndex))[:]
							newNode = f.createArray(eigGroup, GetEigenvectorDatasetName(index), curNode[:])
							newNode._v_attrs.configObject = curFile.root.Eig._v_attrs.configObject

							eigenvalueList.append(curE)
							index += 1

				finally:
					curFile.close()
						
		finally:
			f.close()


