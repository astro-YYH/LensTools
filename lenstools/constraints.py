"""

.. module:: constraints
	:platform: Unix
	:synopsis: This module implements the usual statistical tools you need to calculate cosmological parameters confidence intervals


.. moduleauthor:: Andrea Petri <apetri@phys.columbia.edu>


"""

import numpy as np

##############################################
###########Analysis base class################
##############################################

class Analysis(object):
	
	"""
	The base class of this module; the idea in weak lensing analysis is that one has a set of simulated data, that serves as training model, and then uses that set to fit the observations for the best model parameters.

	:param parameter_set: the points in parameter space that the simulated data cover; the first axis refers to the model, the second to the number of parameters 
	:type parameter_set: array

	:param training_set: the measured feature in each model; the first axis refers to the model, the others to the feature indices (or bins)
	:type training_set: array

	:param observed_set: the measured feature in the data, should be a one dimensional array
	:type observed_set: array

	"""

	_analysis_type = None

	def __init__(self,parameter_set=None,training_set=None,observed_set=None):

		assert self._analysis_type is not None,"Don't instantiate this class directly, use one of its subclasses!"
		
		if parameter_set is not None and training_set is not None:
			assert parameter_set.shape[0] == training_set.shape[0],"There should be one feature for each of the simulated models!"

		self.parameter_set = parameter_set
		self.training_set = training_set
		self.observed_set = observed_set

	def __repr__(self):

		try:
			return "{0} type analysis, based on {1} models spanning a {2}-dimensional parameter space".format(self._analysis_type,self.parameter_set.shape[0],self.parameter_set.shape[1])
		except AttributeError:
			return "{0} type analysis, no models in it yet!".format(self._analysis_type)

	def add_model(self,parameters,feature):

		"""
		Add a model to the training set of the current analysis

		:param parameters: parameter set of the new model
		:type parameters: array

		:param feature: measured feature of the new model
		:type feature: array

		"""

		#If the analysis doesn't have any models, add the first, otherwise simply vstack them
		if self.parameter_set is None:
			
			assert self.training_set is None

			self.parameter_set = parameters.copy()[np.newaxis,:]
			self.training_set = feature.copy()[np.newaxis,:]

		else:

			#Check for input valudity
			assert parameters.shape[0] == self.parameter_set.shape[1]
			assert feature.shape == self.training_set.shape[1:]

			self.parameter_set = np.vstack((self.parameter_set,parameters))
			self.training_set = np.vstack((self.training_set,feature))




###################################################
#############Fisher matrix analysis################
###################################################

class FisherAnalysis(Analysis):

	_analysis_type = "Fisher"
	_fiducial = 0

	"""
	The class handler of a Fisher matrix analysis, inherits from the base class Analysis

	"""

	def set_fiducial(self,n):

		"""
		Sets the fiducial model (with respect to which to compute the derivatives), default is 0 (i.e. self.parameter_set[0])

		:param n: the parameter set you want to use as fiducial
		:type n: int.

		"""

		assert n < self.parameter_set.shape[0],"There are less than {0} models in your analysis".format(n+1)

		self._fiducial = n

	def compute_derivatives(self):

		"""
		Computes the feature derivatives with respect to the parameter sets using one step finite differences; the derivatives are computed with respect to the fiducial parameter set

		:returns: array of shape (p-1,N), where N is the feature dimension and p is the size of the parameter_set

		"""

		assert self.parameter_set.shape[0] > 1,"You need at least 2 models to proceed in a Fisher Analysis!"

		#Keep a list of the varied parameters so far to avoid duplicates
		self._varied_list = list()

		#Calculate the size of the fiducial and non_fiducial features
		derivative_indices = range(self.parameter_set.shape[0])
		derivative_indices.remove(self._fiducial)
		non_fiducial_features = self.training_set[derivative_indices]
		non_fiducial_parameters = self.parameter_set[derivative_indices]

		derivatives = np.zeros(non_fiducial_features.shape)

		#cycle to parameters to calculate derivatives, and do some sanity checks (for example that the parameters are varied one at a time with respect to the fiducial value)
		for n,p in enumerate(derivative_indices):

			#Check that we vary only one parameter at a time
			comparison = non_fiducial_parameters[n] == self.parameter_set[self._fiducial]
			assert comparison.sum() == len(comparison) - 1,"You must vary one parameter at a time!"

			#Check which is the parameter that we varied
			varied_parameter_index = np.where(comparison==False)[0][0]
			assert varied_parameter_index not in self._varied_list,"You cannot vary a parameter twice!!"
			self._varied_list.append(varied_parameter_index)

			#Calculate the finite difference derivative with respect to this parameter
			derivatives[n]  = (non_fiducial_features[n] - self.training_set[self._fiducial]) / (non_fiducial_parameters[n,varied_parameter_index] - self.parameter_set[self._fiducial,varied_parameter_index])

		#set the derivatives attribute and return the result
		self.derivatives = derivatives

		return derivatives

	def get_varied(self):

		"""
		Displays the ordered list of the indices of the varied parameters 

		"""

		assert hasattr(self,"derivatives"),"You must compute the derivatives first!"

		return self._varied_list










