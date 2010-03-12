import unittest
import sys
import os.path

#import pyprop
sys.path.append(os.path.join("..", ".."))
import pyprop.config
import pyprop.problem

#referenced from config file
import pyprop.core as core
import pyprop.modules.discretizations.bspline as bspline
import pyprop.propagator.combined as combined

class PropagatorTestCase(unittest.TestCase):
	def runTest(self):
		conf = pyprop.config.Load("config.ini")
		prop = pyprop.problem.Problem(conf)
		prop.SetupStep()
		prop.AdvanceStep()
		
if __name__ == "__main__":
	unittest.main()
