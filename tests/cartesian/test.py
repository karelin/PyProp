import unittest
import sys
import os.path

#import pyprop
sys.path.append(os.path.join("..", ".."))
import pyprop.config
import pyprop.problem

#referenced from config file
import pyprop.modules.discretizations.fourier as fourier

class PropagatorTestCase(unittest.TestCase):
	def runTest(self):
		conf = pyprop.config.Load("config.ini")
		prop = pyprop.problem.Problem(conf)
		prop.SetupStep()
		prop.AdvanceStep()
		
if __name__ == "__main__":
	unittest.main()
