/*Python wrapper module for convergence map operations;
the method used is the same as in 
http://dan.iel.fm/posts/python-c-extensions/ 

The module is called _topology and it defines the methods below (see docstrings)
*/

#include <complex.h>

#include <Python.h>
#include <numpy/arrayobject.h>

#include "peaks.h"
#include "differentials.h"
#include "minkowski.h"
#include "azimuth.h"

//Python module docstrings 
static char module_docstring[] = "This module provides a python interface for counting peaks in a 2D map";
static char peakCount_docstring[] = "Calculate the peak counts in a 2D map";
static char gradient_docstring[] = "Compute the gradient of a 2D image";
static char hessian_docstring[] = "Compute the hessian of a 2D image"; 
static char minkowski_docstring[] = "Measure the three Minkowski functionals of a 2D image";
static char rfft2_azimuthal_docstring[] = "Measure azimuthal average of Fourier transforms of 2D image";
static char rfft3_azimuthal_docstring[] = "Measure azimuthal average of Fourier transforms of 3D scalar field";

//method declarations
static PyObject *_topology_peakCount(PyObject *self,PyObject *args);
static PyObject *_topology_gradient(PyObject *self,PyObject *args);
static PyObject *_topology_hessian(PyObject *self,PyObject *args);
static PyObject *_topology_minkowski(PyObject *self,PyObject *args);
static PyObject *_topology_rfft2_azimuthal(PyObject *self,PyObject *args);
static PyObject *_topology_rfft3_azimuthal(PyObject *self,PyObject *args);


//_topology method definitions
static PyMethodDef module_methods[] = {

	{"peakCount",_topology_peakCount,METH_VARARGS,peakCount_docstring},
	{"gradient",_topology_gradient,METH_VARARGS,gradient_docstring},
	{"hessian",_topology_hessian,METH_VARARGS,hessian_docstring},
	{"minkowski",_topology_minkowski,METH_VARARGS,minkowski_docstring},
	{"rfft2_azimuthal",_topology_rfft2_azimuthal,METH_VARARGS,rfft2_azimuthal_docstring},
	{"rfft3_azimuthal",_topology_rfft3_azimuthal,METH_VARARGS,rfft3_azimuthal_docstring},
	{NULL,NULL,0,NULL}

} ;

//_topology constructor
PyMODINIT_FUNC init_topology(void){

	PyObject *m = Py_InitModule3("_topology",module_methods,module_docstring);
	if(m==NULL) return;

	/*Load numpy functionality*/
	import_array();

}

//peakCount() implementation
static PyObject *_topology_peakCount(PyObject *self,PyObject *args){

	PyObject *map_obj,*mask_obj,*thresholds_obj; 
	double sigma;

	/*Parse the input tuple*/
	if(!PyArg_ParseTuple(args,"OOOd",&map_obj,&mask_obj,&thresholds_obj,&sigma)){ 
		return NULL;
	}

	/*Interpret the inputs as a numpy arrays*/
	PyObject *map_array = PyArray_FROM_OTF(map_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *thresholds_array = PyArray_FROM_OTF(thresholds_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *mask_array;

	if(map_array==NULL || thresholds_array==NULL){
		
		Py_XDECREF(map_array);
		Py_XDECREF(thresholds_array);

		return NULL;

	}

	if(mask_obj != Py_None){

		mask_array = PyArray_FROM_OTF(mask_obj,NPY_BOOL,NPY_IN_ARRAY);

		if(mask_array==NULL){
			
			Py_DECREF(map_array);
			Py_DECREF(thresholds_array);
			
			return NULL;
		}

	}
	else{

		mask_array = NULL;

	}

	/*Get the size of the map (in pixels)*/
	int Nside = (int)PyArray_DIM(map_array,0);
	/*Get the number of thresholds to output*/
	int Nthreshold = (int)PyArray_DIM(thresholds_array,0);

	/*Prepare a new array object that will hold the peak counts*/
	npy_intp dims[] = {(npy_intp) Nthreshold - 1};
	PyObject *peaks_array = PyArray_ZEROS(1,dims,NPY_DOUBLE,0);

	/*Throw exception if this failed*/
	if(peaks_array==NULL){
		
		Py_DECREF(map_array);
		Py_DECREF(thresholds_array);

		if(mask_array){
			Py_DECREF(mask_array);
		}

		return NULL;
	}

	/*Decide if mask profile is not NULL*/
	unsigned char *mask_profile;
	if(mask_array){
		mask_profile = (unsigned char *)PyArray_DATA(mask_array);
	}
	else{
		mask_profile = NULL;
	}

	/*Call the underlying C function that counts the peaks*/
	peak_count((double *)PyArray_DATA(map_array),mask_profile,Nside,sigma,Nthreshold,(double *)PyArray_DATA(thresholds_array),(double *)PyArray_DATA(peaks_array));

	/*Clean up and return*/
	Py_DECREF(map_array);
	Py_DECREF(thresholds_array);

	if(mask_array){
		Py_DECREF(mask_array);
	}

	return peaks_array;

}

//gradient() implementation
static PyObject *_topology_gradient(PyObject *self,PyObject *args){

	PyObject *map_obj;

	/*Parse the input*/
	if(!PyArg_ParseTuple(args,"O",&map_obj)){ 
		return NULL;
	}

	/*Interpret the input as a numpy array*/
	PyObject *map_array = PyArray_FROM_OTF(map_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	if(map_array==NULL){
		return NULL;
	}

	/*Get the size of the map (in pixels)*/
	int Nside = (int)PyArray_DIM(map_array,0);

	/*Prepare the new array objects that will hold the gradients along x and y*/
	npy_intp dims[] = {(npy_intp) Nside, (npy_intp) Nside};
	PyObject *gradient_x_array = PyArray_SimpleNew(2,dims,NPY_DOUBLE);
	PyObject *gradient_y_array = PyArray_SimpleNew(2,dims,NPY_DOUBLE);

	/*Throw exception if this failed*/
	if(gradient_x_array==NULL || gradient_y_array==NULL){
		
		Py_DECREF(map_array);
		Py_XDECREF(gradient_x_array);
		Py_XDECREF(gradient_y_array);
		return NULL;
	}

	/*Call the underlying C function that computes the gradient*/
	gradient_xy((double *)PyArray_DATA(map_array),(double *)PyArray_DATA(gradient_x_array),(double *)PyArray_DATA(gradient_y_array),Nside);

	/*Prepare a tuple container for the output*/
	PyObject *gradient_output = PyTuple_New(2);
	
	if(PyTuple_SetItem(gradient_output,0,gradient_x_array)){

		Py_DECREF(map_array);
		Py_DECREF(gradient_x_array);
		Py_DECREF(gradient_y_array);
		Py_DECREF(gradient_output);
		return NULL;

	}

	if(PyTuple_SetItem(gradient_output,1,gradient_y_array)){

		Py_DECREF(map_array);
		Py_DECREF(gradient_x_array);
		Py_DECREF(gradient_y_array);
		Py_DECREF(gradient_output);
		return NULL;

	}

	/*Clean up*/
	Py_DECREF(map_array);

	/*Done, now return*/
	return gradient_output;

}

//hessian() implementation
static PyObject *_topology_hessian(PyObject *self,PyObject *args){

	PyObject *map_obj;

	/*Parse the input*/
	if(!PyArg_ParseTuple(args,"O",&map_obj)){ 
		return NULL;
	}

	/*Interpret the input as a numpy array*/
	PyObject *map_array = PyArray_FROM_OTF(map_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	if(map_array==NULL){
		return NULL;
	}

	/*Get the size of the map (in pixels)*/
	int Nside = (int)PyArray_DIM(map_array,0);

	/*Prepare the new array objects that will hold the gradients along x and y*/
	npy_intp dims[] = {(npy_intp) Nside, (npy_intp) Nside};
	PyObject *hessian_xx_array = PyArray_SimpleNew(2,dims,NPY_DOUBLE);
	PyObject *hessian_yy_array = PyArray_SimpleNew(2,dims,NPY_DOUBLE);
	PyObject *hessian_xy_array = PyArray_SimpleNew(2,dims,NPY_DOUBLE);

	/*Throw exception if this failed*/
	if(hessian_xx_array==NULL || hessian_yy_array==NULL || hessian_xy_array==NULL){
		
		Py_DECREF(map_array);
		Py_XDECREF(hessian_xx_array);
		Py_XDECREF(hessian_yy_array);
		Py_XDECREF(hessian_xy_array);
		return NULL;
	}

	/*Call the underlying C function that computes the hessian*/
	hessian((double *)PyArray_DATA(map_array),(double *)PyArray_DATA(hessian_xx_array),(double *)PyArray_DATA(hessian_yy_array),(double *)PyArray_DATA(hessian_xy_array),Nside);

	/*Prepare a tuple container for the output*/
	PyObject *hessian_output = PyTuple_New(3);
	
	if(PyTuple_SetItem(hessian_output,0,hessian_xx_array)){

		Py_DECREF(map_array);
		Py_DECREF(hessian_xx_array);
		Py_DECREF(hessian_yy_array);
		Py_DECREF(hessian_xy_array);
		Py_DECREF(hessian_output);
		return NULL;

	}

	if(PyTuple_SetItem(hessian_output,1,hessian_yy_array)){

		Py_DECREF(map_array);
		Py_DECREF(hessian_xx_array);
		Py_DECREF(hessian_yy_array);
		Py_DECREF(hessian_xy_array);
		Py_DECREF(hessian_output);
		return NULL;

	}

	if(PyTuple_SetItem(hessian_output,2,hessian_xy_array)){

		Py_DECREF(map_array);
		Py_DECREF(hessian_xx_array);
		Py_DECREF(hessian_yy_array);
		Py_DECREF(hessian_xy_array);
		Py_DECREF(hessian_output);
		return NULL;

	}

	/*Clean up*/
	Py_DECREF(map_array);

	/*Done, now return*/
	return hessian_output;

}

//minkowski() implementation
static PyObject *_topology_minkowski(PyObject *self,PyObject *args){

	/*These are the inputs: the map, its gradients, the chosen thresholds and the normalization*/
	PyObject *map_obj,*mask_obj,*grad_x_obj,*grad_y_obj,*hess_xx_obj,*hess_yy_obj,*hess_xy_obj,*thresholds_obj;
	double sigma;

	/*Parse input tuple*/
	if(!PyArg_ParseTuple(args,"OOOOOOOOd",&map_obj,&mask_obj,&grad_x_obj,&grad_y_obj,&hess_xx_obj,&hess_yy_obj,&hess_xy_obj,&thresholds_obj,&sigma)){
		return NULL;
	}

	/*Interpret the parsed objects as numpy arrays*/
	PyObject *map_array = PyArray_FROM_OTF(map_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *grad_x_array = PyArray_FROM_OTF(grad_x_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *grad_y_array = PyArray_FROM_OTF(grad_y_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *hess_xx_array = PyArray_FROM_OTF(hess_xx_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *hess_yy_array = PyArray_FROM_OTF(hess_yy_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *hess_xy_array = PyArray_FROM_OTF(hess_xy_obj,NPY_DOUBLE,NPY_IN_ARRAY);
	PyObject *thresholds_array = PyArray_FROM_OTF(thresholds_obj,NPY_DOUBLE,NPY_IN_ARRAY);

	/*Check if the previous operation failed*/
	int fail = (map_array==NULL || grad_x_array==NULL || grad_y_array==NULL || hess_xx_array==NULL || hess_yy_array==NULL || hess_xy_array==NULL || thresholds_array==NULL);
	if(fail){
		
		Py_XDECREF(map_array);
		Py_XDECREF(grad_x_array);
		Py_XDECREF(grad_y_array);
		Py_XDECREF(hess_xx_array);
		Py_XDECREF(hess_yy_array);
		Py_XDECREF(hess_xy_array);
		Py_XDECREF(thresholds_array);

		return NULL;

	}

	/*Decide if creating a mask array or not*/
	PyObject *mask_array;
	if(mask_obj != Py_None){

		mask_array = PyArray_FROM_OTF(mask_obj,NPY_BOOL,NPY_IN_ARRAY);
		if(mask_array==NULL){

			Py_DECREF(map_array);
			Py_DECREF(grad_x_array);
			Py_DECREF(grad_y_array);
			Py_DECREF(hess_xx_array);
			Py_DECREF(hess_yy_array);
			Py_DECREF(hess_xy_array);
			Py_DECREF(thresholds_array);

			return NULL;

		}

	}
	else{

		mask_array = NULL;
	}

	/*Get the size of the map (in pixels)*/
	int Nside = (int)PyArray_DIM(map_array,0);
	/*Get the number of excurion sets*/
	int Nthreshold = (int)PyArray_DIM(thresholds_array,0);

	/*Prepare the new array objects that will hold the measured minkowski functionals*/
	npy_intp dims[] = {Nthreshold - 1};
	PyObject *mink0_array = PyArray_ZEROS(1,dims,NPY_DOUBLE,0);
	PyObject *mink1_array = PyArray_ZEROS(1,dims,NPY_DOUBLE,0);
	PyObject *mink2_array = PyArray_ZEROS(1,dims,NPY_DOUBLE,0);
	/*Prepare a tuple container for the output*/
	PyObject *mink_output = PyTuple_New(3);

	/*Check if something failed*/
	fail = (mink0_array==NULL || mink1_array==NULL || mink2_array==NULL || mink_output==NULL);
	if(fail){

		Py_DECREF(map_array);
		Py_DECREF(grad_x_array);
		Py_DECREF(grad_y_array);
		Py_DECREF(hess_xx_array);
		Py_DECREF(hess_yy_array);
		Py_DECREF(hess_xy_array);
		Py_DECREF(thresholds_array);

		if(mask_array){
			Py_DECREF(mask_array);
		}

		Py_XDECREF(mink0_array);
		Py_XDECREF(mink1_array);
		Py_XDECREF(mink2_array);
		Py_XDECREF(mink_output);

		return NULL;

	}

	/*Decide if mask_profile is null or not*/
	unsigned char *mask_profile;
	if(mask_array){
		mask_profile = (unsigned char *)PyArray_DATA(mask_array);
	}
	else{
		mask_profile = NULL;
	}

	/*Call the underlying C function that measures the Minkowski functionals*/
	minkowski_functionals((double *)PyArray_DATA(map_array),mask_profile,Nside,sigma,(double *)PyArray_DATA(grad_x_array),(double *)PyArray_DATA(grad_y_array),(double *)PyArray_DATA(hess_xx_array),(double *)PyArray_DATA(hess_yy_array),(double *)PyArray_DATA(hess_xy_array),Nthreshold,(double *)PyArray_DATA(thresholds_array),(double *)PyArray_DATA(mink0_array),(double *)PyArray_DATA(mink1_array),(double *)PyArray_DATA(mink2_array));

	/*Add the results to the tuple output*/
	if(PyTuple_SetItem(mink_output,0,mink0_array)){

		Py_DECREF(map_array);
		Py_DECREF(grad_x_array);
		Py_DECREF(grad_y_array);
		Py_DECREF(hess_xx_array);
		Py_DECREF(hess_yy_array);
		Py_DECREF(hess_xy_array);
		Py_DECREF(thresholds_array);

		if(mask_array){
			Py_DECREF(mask_array);
		}

		Py_DECREF(mink0_array);
		Py_DECREF(mink1_array);
		Py_DECREF(mink2_array);
		Py_DECREF(mink_output);

		return NULL;

	}

	if(PyTuple_SetItem(mink_output,1,mink1_array)){

		Py_DECREF(map_array);
		Py_DECREF(grad_x_array);
		Py_DECREF(grad_y_array);
		Py_DECREF(hess_xx_array);
		Py_DECREF(hess_yy_array);
		Py_DECREF(hess_xy_array);
		Py_DECREF(thresholds_array);

		if(mask_array){
			Py_DECREF(mask_array);
		}

		Py_DECREF(mink0_array);
		Py_DECREF(mink1_array);
		Py_DECREF(mink2_array);
		Py_DECREF(mink_output);

		return NULL;

	}

	if(PyTuple_SetItem(mink_output,2,mink2_array)){

		Py_DECREF(map_array);
		Py_DECREF(grad_x_array);
		Py_DECREF(grad_y_array);
		Py_DECREF(hess_xx_array);
		Py_DECREF(hess_yy_array);
		Py_DECREF(hess_xy_array);
		Py_DECREF(thresholds_array);

		if(mask_array){
			Py_DECREF(mask_array);
		}

		Py_DECREF(mink0_array);
		Py_DECREF(mink1_array);
		Py_DECREF(mink2_array);
		Py_DECREF(mink_output);

		return NULL;

	}

	/*Clean up*/
	Py_DECREF(map_array);
	Py_DECREF(grad_x_array);
	Py_DECREF(grad_y_array);
	Py_DECREF(hess_xx_array);
	Py_DECREF(hess_yy_array);
	Py_DECREF(hess_xy_array);
	Py_DECREF(thresholds_array);

	if(mask_array){
		Py_DECREF(mask_array);
	}

	/*Done, return Minkowski tuple*/
	return mink_output;

}

//rfft2_azimuthal() implementation
static PyObject *_topology_rfft2_azimuthal(PyObject *self,PyObject *args){

	/*These are the inputs: the Fourier transforms of the two maps, the side angle of the real space map, the bin extremes at which calculate the azimuthal averages*/
	PyObject *ft_map1_obj,*ft_map2_obj,*lvalues_obj;
	double map_angle_degrees;

	/*Parse input tuple*/
	if(!PyArg_ParseTuple(args,"OOdO",&ft_map1_obj,&ft_map2_obj,&map_angle_degrees,&lvalues_obj)){
		return NULL;
	}

	/*Interpret the parsed objects as numpy arrays*/
	PyObject *ft_map1_array = PyArray_FROM_OTF(ft_map1_obj,NPY_COMPLEX128,NPY_IN_ARRAY);
	PyObject *ft_map2_array = PyArray_FROM_OTF(ft_map2_obj,NPY_COMPLEX128,NPY_IN_ARRAY);
	PyObject *lvalues_array = PyArray_FROM_OTF(lvalues_obj,NPY_DOUBLE,NPY_IN_ARRAY);

	/*Check if anything failed*/
	if(ft_map1_array==NULL || lvalues_array==NULL || ft_map2_array==NULL){

		Py_XDECREF(ft_map1_array);
		Py_XDECREF(ft_map2_array);
		Py_XDECREF(lvalues_array);

		return NULL;
	}

	/*Get the size of the map fourier transform*/
	int Nside_x = (int)PyArray_DIM(ft_map1_array,0);
	int Nside_y = (int)PyArray_DIM(ft_map1_array,1);

	/*Get the number of multipole moment bin edges*/
	int Nvalues = (int)PyArray_DIM(lvalues_array,0);

	/*Build the array that will contain the output*/
	npy_intp dims[] = {(npy_intp) Nvalues - 1};
	PyObject *power_array = PyArray_ZEROS(1,dims,NPY_DOUBLE,0);

	/*Check for failure*/
	if(power_array==NULL){

		Py_DECREF(ft_map1_array);
		Py_DECREF(ft_map2_array);
		Py_DECREF(lvalues_array);

		return NULL;
	}

	/*Call the C backend azimuthal average function*/
	if(azimuthal_rfft2((double _Complex *)PyArray_DATA(ft_map1_array),(double _Complex *)PyArray_DATA(ft_map2_array),Nside_x,Nside_y,map_angle_degrees,Nvalues,(double *)PyArray_DATA(lvalues_array),(double *)PyArray_DATA(power_array))){

		Py_DECREF(ft_map1_array);
		Py_DECREF(ft_map2_array);
		Py_DECREF(lvalues_array);
		Py_DECREF(power_array);

		return NULL;

	}

	/*If the call succeeded cleanup and return*/
	Py_DECREF(ft_map1_array);
	Py_DECREF(ft_map2_array);
	Py_DECREF(lvalues_array);

	return power_array;

}

//rfft3_azimuthal() implementation
static PyObject *_topology_rfft3_azimuthal(PyObject *self,PyObject *args){

	/*These are the inputs: the Fourier transforms of the two maps, the size of the pixel in k space, the k bin extremes at which calculate the azimuthal averages*/
	PyObject *ft_map1_obj,*ft_map2_obj,*kvalues_obj;
	double kpixX,kpixY,kpixZ;

	/*Parse input tuple*/
	if(!PyArg_ParseTuple(args,"OOdddO",&ft_map1_obj,&ft_map2_obj,&kpixX,&kpixY,&kpixZ,&kvalues_obj)){
		return NULL;
	}

	/*Interpret the parsed objects as numpy arrays*/
	PyObject *ft_map1_array = PyArray_FROM_OTF(ft_map1_obj,NPY_COMPLEX128,NPY_IN_ARRAY);
	PyObject *ft_map2_array = PyArray_FROM_OTF(ft_map2_obj,NPY_COMPLEX128,NPY_IN_ARRAY);
	PyObject *kvalues_array = PyArray_FROM_OTF(kvalues_obj,NPY_DOUBLE,NPY_IN_ARRAY);

	/*Check if anything failed*/
	if(ft_map1_array==NULL || kvalues_array==NULL || ft_map2_array==NULL){

		Py_XDECREF(ft_map1_array);
		Py_XDECREF(ft_map2_array);
		Py_XDECREF(kvalues_array);

		return NULL;
	}

	/*Get the size of the map fourier transform*/
	int Nside_x = (int)PyArray_DIM(ft_map1_array,0);
	int Nside_y = (int)PyArray_DIM(ft_map1_array,1);
	int Nside_z = (int)PyArray_DIM(ft_map1_array,2);

	/*Get the number of k bin edges*/
	int Nvalues = (int)PyArray_DIM(kvalues_array,0);

	/*Build the arrays that will contain the output:power spectrum and hits*/
	npy_intp dims[] = {(npy_intp) Nvalues - 1};
	PyObject *power_array = PyArray_ZEROS(1,dims,NPY_DOUBLE,0);
	PyObject *hits_array = PyArray_ZEROS(1,dims,NPY_LONG,0);

	/*Check for failure*/
	if(power_array==NULL || hits_array==NULL){

		Py_DECREF(ft_map1_array);
		Py_DECREF(ft_map2_array);
		Py_DECREF(kvalues_array);
		Py_XDECREF(power_array);
		Py_XDECREF(hits_array);

		return NULL;
	}

	/*Call the C backend azimuthal average function*/
	if(azimuthal_rfft3((double _Complex *)PyArray_DATA(ft_map1_array),(double _Complex *)PyArray_DATA(ft_map2_array),Nside_x,Nside_y,Nside_z,kpixX,kpixY,kpixZ,Nvalues,(double *)PyArray_DATA(kvalues_array),(double *)PyArray_DATA(power_array),(long *)PyArray_DATA(hits_array))){

		Py_DECREF(ft_map1_array);
		Py_DECREF(ft_map2_array);
		Py_DECREF(kvalues_array);
		Py_DECREF(power_array);
		Py_DECREF(hits_array);

		return NULL;

	}

	/*If the call succeeded, build the output tuple and quit*/
	PyObject *output = PyTuple_New(2);
	if(PyTuple_SetItem(output,0,hits_array)){

		Py_DECREF(ft_map1_array);
		Py_DECREF(ft_map2_array);
		Py_DECREF(kvalues_array);
		Py_DECREF(power_array);
		Py_DECREF(hits_array);

		return NULL;

	}

	if(PyTuple_SetItem(output,1,power_array)){

		Py_DECREF(ft_map1_array);
		Py_DECREF(ft_map2_array);
		Py_DECREF(kvalues_array);
		Py_DECREF(power_array);
		Py_DECREF(hits_array);

		return NULL;

	}

	//Cleanup and return
	Py_DECREF(ft_map1_array);
	Py_DECREF(ft_map2_array);
	Py_DECREF(kvalues_array);

	return output;
}

