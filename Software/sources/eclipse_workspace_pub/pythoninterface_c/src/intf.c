// SH: made some corrections to networklist routines so that python example works / but some functions are still wrong, e.g. networklist_remove

#include "Python.h"
//#include "/usr/lib64/python2.7/site-packages/numpy/core/include/numpy/arrayobject.h"
#include "arrayobject.h"

#include "Network_c.h"
#include "NetworkSerialization.h"
#include <string.h>

//constants:
const int NETTYPE_GMLVQ = GMLVQ;
const int NETTYPE_GRLVQ = GRLVQ;
const int NETTYPE_LGMLVQ = LGMLVQ;
const int NETTYPE_LGRLVQ = LGRLVQ;
const int NETTYPE_GLVQ = GLVQ;

const int NETWORK_LEARNRATE = false;
const int NETWORK_LEARNRATE_PROTOTYPE = true;

struct Networklist_struct {
	char* name;
	Network_c* netref;
	struct Networklist_struct* prev;
	struct Networklist_struct* next;
};
typedef struct Networklist_struct Networklist;

Networklist *networkholder = NULL;

// SH: curr_pos = curr_pos->next was missing + other corrections
Networklist* networklist_find(const char* name) {
	Networklist *curr_pos = networkholder;

	while (curr_pos != NULL) {
		if (name) {
			if (strcmp(name, curr_pos->name) == 0) {
				return curr_pos;
			}
		}

		// return last entry if no name given
		if (!name && curr_pos->next == NULL) {
			return curr_pos;
		}
		curr_pos = curr_pos->next;
	}

	return NULL;
}

Network_c *network_access(const char* name) {
	Networklist *resentry = networklist_find(name);

	if (resentry)
		return resentry->netref;
	return NULL;

}

// SH: produced segfault in case of empty list / added if listend and strcpy of name
void networklist_add(const char* name, Network_c* net) {
	int stringlen = strlen(name);
	Networklist *nentry = (Networklist*) malloc(sizeof(Networklist));
	nentry->name = (char*) malloc(sizeof(char) * stringlen + 1);
	strcpy(nentry->name, name);
	nentry->netref = net;
	nentry->next = NULL;
	nentry->prev = NULL;
	Networklist *listend = networklist_find(NULL);
	if (listend) {
		nentry->prev = listend;
		listend->next = nentry;
	} else {
		networkholder = nentry;
	}
}

void networklist_remove(const char* name) {
	Networklist* listend = networklist_find(name);

	if (listend) {
		if (listend->prev)
			listend->prev->next = listend->next;
		if (listend->next)
			listend->next->prev = listend->prev;
		free(listend->name);
//XXVL		free(listend);

	}
}

//XXVL manual init of prototypes needed!



static PyObject *py_add_prototype(PyObject *self, PyObject *args){
	char *name;

	PyObject *data;
	int label =-1;


	if (!PyArg_ParseTuple(args, "sOi", &name, &data, &label))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	//create c-arrays:
	PyArrayObject *mat_data;
	mat_data = (PyArrayObject *) PyArray_ContiguousFromObject(data,
			PyArray_DOUBLE, 1, 2);
	if ((NULL == mat_data)
			|| (((PyArrayObject *) data)->descr->type_num != NPY_DOUBLE)) {
		printf(
				"Error: data must be a 2-D double mat, each row is one datasample\n");
		return NULL;
	}
	double *carr_data = (double*) (mat_data->data);
	int protoDimensions = mat_data->dimensions[0];
	if ((unsigned int) protoDimensions != network_getDimensionality(net)) {
		printf(
				"Error: prototype dimensions %i must equal dimensionality of net(%i)\n",
				protoDimensions, network_getDimensionality(net));
		return NULL;
	}

	//PROTOFRMT* data, int* label, unsigned int data_rows=1, bool random_selection=false
	network_addPrototype(net, label, carr_data);

	Py_DECREF(mat_data);
	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_add_prototype__doc__,
		"name, data, label");
/*
 * Parameter:
 * 		name (string)	:identifier
 * 		dimensionality(int)
 * 		type (GRLVQ, GMLVQ ...) - one of constants
 * 		learnrate_per_node(int) - one of constants
 * 		x of classes 0 to x-1(int)
 * 		y prototypes per class(int)
 * 		num of trainsteps for learnrate adoption
 * 		(opt)random initialization(bool) or init by first x*y samples
 * 		(opt)treads per dimension(int)
 *
 * 	returns true on success or false if name not available
 */
static PyObject *py_create_network(PyObject *self, PyObject *args) {
	const char *name;
	int dimensionality, type, learnrate_per_node, trainsteps, threads, num_of_classes,
			protos_per_class;

	threads = 1;
	num_of_classes = 0;
	protos_per_class = 1;
	if (!PyArg_ParseTuple(args, "siiii|iii", &name, &dimensionality, &type,
			&learnrate_per_node, &trainsteps, &threads,
			&num_of_classes, &protos_per_class))
		return NULL;

	if (network_access(name))
		return PyInt_FromLong(false);

	Network_c *net = network_create((enum Mode) type, dimensionality,
			learnrate_per_node, trainsteps,
			threads);
	srand(0);
	if (num_of_classes >0 && protos_per_class > 0) {
		printf("random init of prototypes\n");
		int obj, proto;
		for (obj = 0; obj < num_of_classes; obj++) {
			for (proto = 0; proto < protos_per_class; proto++) {
				network_addPrototype(net, obj, NULL);
			}
		}
	}

	networklist_add(name, net);

	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_create_network__doc__,
		"name, dimensionality, type, learnrate_per_node, # of classes, prototypes per class, trainsteps, (opt) random init, (opt) thread per dimensionality, just select the x first dimensions for euclidean distance - inital");

/*
 * Parameter:
 * 		name(string)
 *
 * returns true if network was available
 */
static PyObject *py_delete_network(PyObject *self, PyObject *args) {
	const char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	network_clear(net);
	free(net);
    networklist_remove(name);

	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_delete_network__doc__, "name");

static PyObject *py_get_weight_network(PyObject *self, PyObject *args) {
	const char *name;
	int protonr;

	if (!PyArg_ParseTuple(args, "si", &name, &protonr))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	npy_intp dims = network_getDimensionality(net);

	PyArrayObject *weights = (PyArrayObject *) PyArray_SimpleNew(1, &dims,
			PyArray_DOUBLE);
	PyArrayObject *mat_weights;
	mat_weights = (PyArrayObject *) PyArray_ContiguousFromObject(
			(PyObject* )weights, PyArray_DOUBLE, 1, 1);
	if (NULL == mat_weights) {
		printf("Error: weights must be a 1-D double mat\n");
		return NULL;
	}
	double *carr_weights = (double*) (mat_weights->data);

	network_getPrototypeData(net, protonr, carr_weights);

	Py_DECREF(weights);

	return Py_BuildValue("O", mat_weights);
}
PyDoc_STRVAR(py_get_weight_network__doc__, "name, protonr");

static PyObject *py_set_learnrate_start(PyObject *self, PyObject *args) {
	const char *name;
	double val;

	if (!PyArg_ParseTuple(args, "sd", &name, &val))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	network_setLearnrateStart(net, val);

	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_set_learnrate_start__doc__, "name, val");

static PyObject *py_set_learnrate_metricWeights_start(PyObject *self, PyObject *args) {
	char *name;
	double val;

	if (!PyArg_ParseTuple(args, "sd", &name, &val))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	network_setLearnrateMetricWeightsStart(net, val);

	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_set_learnrate_metricWeights_start__doc__, "name, val");

static PyObject *py_get_metricWeight_network(PyObject *self, PyObject *args) {
	char *name;
	int protonr;

	if (!PyArg_ParseTuple(args, "si", &name, &protonr))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	npy_intp dims[2];
	dims[1] = dims[0] = network_getDimensionality(net);
	int metricWeightdims = network_getMetricWeightsLen(net);

	int dimensions = 1;
	if (metricWeightdims != dims[0])
		dimensions = 2;

	PyArrayObject *metricWeights = (PyArrayObject *) PyArray_SimpleNew(dimensions,
			dims, PyArray_DOUBLE);
	PyArrayObject *mat_metricWeights;
	mat_metricWeights = (PyArrayObject *) PyArray_ContiguousFromObject(
			(PyObject* )metricWeights, PyArray_DOUBLE, dimensions, dimensions);
	if (NULL == mat_metricWeights) {
		printf("Error: weights must be a 2/1-D double mat\n");
		return NULL;
	}
	double *carr_metricWeights = (double*) (mat_metricWeights->data);

	network_getMetricWeights(net, protonr, carr_metricWeights);

	Py_DECREF(metricWeights);

	return Py_BuildValue("O", mat_metricWeights);
}
PyDoc_STRVAR(py_get_metricWeight_network__doc__, "name, protonr");

static PyObject *py_get_eingenvectors_network(PyObject *self, PyObject *args) {
	char *name;
	int protonr;

	if (!PyArg_ParseTuple(args, "si", &name, &protonr))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	npy_intp dims[2];
	dims[1] = dims[0] = network_getDimensionality(net);

	int dimensions = 2;

	PyArrayObject *evecs = (PyArrayObject *) PyArray_SimpleNew(dimensions, dims,
			PyArray_DOUBLE);
	PyArrayObject *mat_evecs;
	mat_evecs = (PyArrayObject *) PyArray_ContiguousFromObject(
			(PyObject* )evecs, PyArray_DOUBLE, dimensions, dimensions);
	if (NULL == mat_evecs) {
		printf("Error: weights must be a 2/1-D double mat\n");
		return NULL;
	}
	double *carr_evecs = (double*) (mat_evecs->data);

	network_getEigenvectors(net, protonr, carr_evecs);

	Py_DECREF(evecs);

	return Py_BuildValue("O", mat_evecs);
}
PyDoc_STRVAR(py_get_eingenvectors_network__doc__, "name, protonr");

static PyObject *py_train_network(PyObject *self, PyObject *args) {
	char *name;

	PyObject *data, *label;
	bool adaptMetricWeights = true;

	if (!PyArg_ParseTuple(args, "sOO|i", &name, &data, &label, &adaptMetricWeights))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	//create c-arrays:
	PyArrayObject *mat_data;
	mat_data = (PyArrayObject *) PyArray_ContiguousFromObject(data,
			PyArray_DOUBLE, 1, 2);
	if ((NULL == mat_data)
			|| (((PyArrayObject *) data)->descr->type_num != NPY_DOUBLE)) {
		printf(
				"Error: data must be a 2-D double mat, each row is one datasample\n");
		return NULL;
	}
	double *carr_data = (double*) (mat_data->data);
	int carr_data_rows = mat_data->dimensions[0];
	int carr_data_cols = mat_data->dimensions[1];

	if (mat_data->nd == 1) {
		carr_data_cols = carr_data_rows;
		carr_data_rows = 1;
	}

	if ((unsigned int) carr_data_cols != network_getDimensionality(net)) {
		printf(
				"Error: data's row length(%i must equal dimensionality of nodes(%i)\n",
				carr_data_cols, network_getDimensionality(net));
		return NULL;
	}

	PyArrayObject *mat_labels;
	mat_labels = (PyArrayObject *) PyArray_ContiguousFromObject(label,
			PyArray_INT, 1, 1);
	if ( NULL == mat_labels) {
		printf("Error: label must be a 1-D double mat\n");
		return NULL;
	}
	int *carr_label = (int*) (mat_labels->data);
	int carr_label_rows = mat_labels->dimensions[0];

	if (carr_data_rows != carr_label_rows) {
		printf(
				"Error: data's number of rows must equal the number of labels!\n");
		return NULL;
	}
	//PROTOFRMT* data, int* label, unsigned int data_rows=1, bool random_selection=false
	double delta = network_train(net, carr_data, carr_label,
			carr_data_rows, adaptMetricWeights);

	Py_DECREF(mat_data);
	Py_DECREF(mat_labels);

	return PyFloat_FromDouble(delta);
}
PyDoc_STRVAR(py_train_network__doc__,
		"name, data 2D-arr, label 1D-arr, adapt_metric_weights. Returns medium weight shift");

static PyObject *py_incremental_train_network(PyObject *self, PyObject *args) {
	char *name;

	PyObject *data, *label;
	int doInsertions, g_max;

	bool adaptMetricWeights = true;

	if (!PyArg_ParseTuple(args, "sOOii|i", &name, &data, &label,
			&doInsertions, &g_max, &adaptMetricWeights))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	//create c-arrays:
	PyArrayObject *mat_data;
	mat_data = (PyArrayObject *) PyArray_ContiguousFromObject(data,
			PyArray_DOUBLE, 1, 2);
	if ((NULL == mat_data)
			|| (((PyArrayObject *) data)->descr->type_num != NPY_DOUBLE)) {
		printf(
				"Error: data must be a 2-D double mat, each row is one datasample\n");
		return NULL;
	}
	double *carr_data = (double*) (mat_data->data);
	int carr_data_rows = mat_data->dimensions[0];
	int carr_data_cols = mat_data->dimensions[1];

	if (mat_data->nd == 1) {
		carr_data_cols = carr_data_rows;
		carr_data_rows = 1;
	}

	if ((unsigned int) carr_data_cols != network_getDimensionality(net)) {
		printf(
				"Error: data's row length(%i must equal dimensionality of nodes(%i)\n",
				carr_data_cols, network_getDimensionality(net));
		return NULL;
	}

	PyArrayObject *mat_labels;
	mat_labels = (PyArrayObject *) PyArray_ContiguousFromObject(label,
			PyArray_INT, 1, 1);
	if ( NULL == mat_labels) {
		printf("Error: label must be a 1-D double mat\n");
		return NULL;
	}
	int *carr_label = (int*) (mat_labels->data);
	int carr_label_rows = mat_labels->dimensions[0];

	if (carr_data_rows != carr_label_rows) {
		printf(
				"Error: data's number of rows must equal the number of labels!\n");
		return NULL;
	}
	//PROTOFRMT* data, int* label, bool is_failure_increment=false, unsigned int g_max=10, unsigned int data_rows=1, bool random_selection=false
	double perf;
	double delta = network_trainIncremental(net, carr_data, carr_label,
			doInsertions, g_max, carr_data_rows, &perf, adaptMetricWeights);

	Py_DECREF(mat_data);
	Py_DECREF(mat_labels);

	return Py_BuildValue("dd", delta, perf);
}
PyDoc_STRVAR(py_incremental_train_network__doc__,
		"name, data 2D-arr, label 1D-arr, fail_icrement, g_max, trainmode - const, random_selection. Returns medium weight shift");

static PyObject *py_process_network(PyObject *self, PyObject *args) {
	char *name;
	PyObject *data;

	if (!PyArg_ParseTuple(args, "sO", &name, &data))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	//create c-arrays:
	PyArrayObject *mat_data;
	mat_data = (PyArrayObject *) PyArray_ContiguousFromObject(data,
			PyArray_DOUBLE, 1, 2);
	if ((NULL == mat_data)
			|| (((PyArrayObject *) data)->descr->type_num != NPY_DOUBLE)) {
		printf(
				"Error: data must be a 2-D double mat, each row is one datasample\n");
		return NULL;
	}
	double *carr_data = (double*) (mat_data->data);
	npy_intp carr_data_rows = mat_data->dimensions[0];
	npy_intp carr_data_cols = mat_data->dimensions[1];

	if (mat_data->nd == 1) {
		carr_data_cols = carr_data_rows;
		carr_data_rows = 1;
	}

	if ((unsigned int) carr_data_cols != network_getDimensionality(net)) {
		printf(
				"Error: data's row length(%li must equal dimensionality of nodes(%u)\n",
				carr_data_cols, network_getDimensionality(net));
		return NULL;
	}

	PyArrayObject *label = (PyArrayObject *) PyArray_SimpleNew(1,
			&carr_data_rows, PyArray_INT);
	PyArrayObject *indices = (PyArrayObject *) PyArray_SimpleNew(1,
			&carr_data_rows, PyArray_INT);
	PyArrayObject *mat_labels;
	PyArrayObject *mat_indices;
	mat_labels = (PyArrayObject *) PyArray_ContiguousFromObject(
			(PyObject* )label, PyArray_INT, 1, 1);
	mat_indices = (PyArrayObject *) PyArray_ContiguousFromObject(
			(PyObject* )indices, PyArray_INT, 1, 1);
	if (NULL == mat_labels) {
		printf("Error: label must be a 1-D double mat\n");
		return NULL;
	}
	if (NULL == mat_labels) {
		printf("Error: indices must be a 1-D double mat\n");
		return NULL;
	}
	int *carr_label = (int*) (mat_labels->data);
	unsigned int *carr_indices = (unsigned int*) (mat_indices->data);
	int carr_label_rows = mat_labels->dimensions[0];

	if (carr_data_rows != carr_label_rows) {
		printf(
				"Error: data's number of rows must equal the number of labels!\n");
		return NULL;
	}

	network_processData(net, carr_data, carr_label, carr_data_rows,
			carr_indices);

	Py_DECREF(mat_data);
	Py_DECREF(label);
	Py_DECREF(indices);

	return Py_BuildValue("OO", mat_labels, mat_indices);
}
PyDoc_STRVAR(py_process_network__doc__,
		"name, data 2D-arr. Returns label result matrix");

static PyObject *py_save_network(PyObject *self, PyObject *args) {
	const char *name, *file;

	if (!PyArg_ParseTuple(args, "ss", &name, &file))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(false);

	network_saveFile(net, (char*) file);

	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_save_network__doc__, "name, filename");

static PyObject *py_get_numofprototypes_network(PyObject *self, PyObject *args) {
	char *name;

	if (!PyArg_ParseTuple(args, "s", &name))
		return NULL;

	Network_c *net = network_access(name);
	if (!net)
		return PyInt_FromLong(-1);

	return PyInt_FromLong(network_getPrototypes(net));
}
PyDoc_STRVAR(py_get_numofprototypes_network__doc__, "name");

//int getPrototypeLabel(unsigned int protonr)
static PyObject *py_get_prototypelabel_network(PyObject *self, PyObject *args) {
	char *name;
	int protonr;

	if (!PyArg_ParseTuple(args, "si", &name, &protonr))
		return NULL;

	Network_c *net = network_access(name);

	if (!net)
		return PyInt_FromLong(-1);

	return PyInt_FromLong(network_getPrototypeLabel(net, protonr));
}
PyDoc_STRVAR(py_get_prototypelabel_network__doc__, "name, nr -> label");

static PyObject *py_load_network(PyObject *self, PyObject *args) {
	const char *name, *file;

	if (!PyArg_ParseTuple(args, "ss", &name, &file))
		return NULL;

	if (network_access(name))
		return PyInt_FromLong(false);

	Network_c *net = network_createFile((char*) file);
	networklist_add(name, net);

	return PyInt_FromLong(true);
}
PyDoc_STRVAR(py_load_network__doc__, "name, filename");

/* make this static if you don't want other code to call this function */
/* I don't make it static because want to access this via ctypes */
/* static */
int iterate_point(double x0, double y0, int max_iterations) {
	int iteration = 0;
	double x = x0, y = y0, x2 = x * x, y2 = y * y;

	while (x2 + y2 < 4 && iteration < max_iterations) {
		y = 2 * x * y + y0;
		x = x2 - y2 + x0;
		x2 = x * x;
		y2 = y * y;
		iteration++;
	}
	return iteration;
}

/* The module doc string */
PyDoc_STRVAR(mandelbrot__doc__, "Mandelbrot point evalutation kernel");

/* The function doc string */
PyDoc_STRVAR(iterate_point__doc__,
		"x,y,max_iterations -> iteration count at that point, up to max_iterations");

PyDoc_STRVAR(test_mat__doc__, "square input mat - test");

/* The wrapper to the underlying C function */
static PyObject *
py_iterate_point(PyObject *self, PyObject *args) {
	double x = 0, y = 0;
	int iterations, max_iterations = 1000;
	/* "args" must have two doubles and may have an integer */
	/* If not specified, "max_iterations" remains unchanged; defaults to 1000 */
	/* The ':iterate_point' is for error messages */
	if (!PyArg_ParseTuple(args, "dd|i:iterate_point", &x, &y, &max_iterations))
		return NULL;
	/* Verify the parameters are correct */
	if (max_iterations < 0)
		max_iterations = 0;

	/* Call the C function */
	iterations = iterate_point(x, y, max_iterations);

	/* Convert from a C integer value to a Python integer instance */
	return PyInt_FromLong((long) iterations);
}

/* ==== Allocate a double *vector (vec of pointers) ======================
 Memory is Allocated!  See void free_Carray(double ** )                  */
double **ptrvector(long n) {
	double **v;
	v = (double **) malloc((size_t) (n * sizeof(double)));
	if (!v) {
		printf("In **ptrvector. Allocation of memory for double array failed.");
		exit(0);
	}
	return v;
}
/* ==== Free a double *vector (vec of pointers) ========================== */
void free_Carrayptrs(double **v) {
	free((char*) v);
}

/* ==== Check that PyArrayObject is a double (Float) type and a matrix ==============
 return 1 if an error and raise exception */
int not_doublematrix(PyArrayObject *mat) {
	if (mat->descr->type_num != NPY_DOUBLE || mat->nd != 2) {
		PyErr_SetString(PyExc_ValueError,
				"In not_doublematrix: array must be of type Float and 2 dimensional (n x m).");
		return 1;
	}
	return 0;
}

/* ==== Create Carray from PyArray ======================
 Assumes PyArray is contiguous in memory.
 Memory is allocated!                                    */
double **pymatrix_to_Carrayptrs(PyArrayObject *arrayin) {
	double **c, *a;
	int i, n, m;

	n = arrayin->dimensions[0];
	m = arrayin->dimensions[1];
	c = ptrvector(n);
	a = (double *) arrayin->data; /* pointer to arrayin data as double */
	for (i = 0; i < n; i++) {
		c[i] = a + i * m;
	}
	return c;
}

static PyObject *py_test_mat(PyObject *self, PyObject *args) {
	PyObject *in;
	PyArrayObject *matin, *matout;
	double **cin, **cout, dfactor;
	int i, j, n, m, dims[2], ifactor;

	/* Parse tuples separately since args will differ between C fcns */
	if (!PyArg_ParseTuple(args, "Oid", &in, &ifactor, &dfactor)) {
		printf("getting array failed!\n");
		return NULL;
	} else
		printf("getting array sucess!\n");

	matin = (PyArrayObject *) PyArray_ContiguousFromObject(in, PyArray_DOUBLE,
			2, 2);
	if (NULL == matin) {
		printf("not double or 2d-mat!\n");
		return NULL;
	}

	double *in_arr = (double*) (matin->data);

	int d0 = matin->dimensions[0];
	int d1 = matin->dimensions[1];

	printf("d0 %i (%li)  d1 %i (%li) \n", d0, matin->strides[0], d1,
			matin->strides[0]);

	for (i = 0; i < d0; i++) {
		for (j = 0; j < d1; j++) {
			printf("%f ", in_arr[j + i * d1]++);
		}
		printf("\n");
	}

	Py_DECREF(matin);

	return Py_BuildValue("sidO", "test", 3, 5.5, matin);

	/* Check that object input is 'double' type and a matrix
	 Not needed if python wrapper function checks before call to this routine */

	/* Get the dimensions of the input */
	n = dims[0] = matin->dimensions[0];
	m = dims[1] = matin->dimensions[1];

	/* Make a new double matrix of same dims */
	matout = (PyArrayObject *) PyArray_FromDims(2, dims, NPY_DOUBLE);

	/* Change contiguous arrays into C ** arrays (Memory is Allocated!) */
	cin = pymatrix_to_Carrayptrs(matin);
	cout = pymatrix_to_Carrayptrs(matout);

	/* Do the calculation. */
	for (i = 0; i < n; i++) {
		for (j = 0; j < m; j++) {
			cout[i][j] = ifactor * dfactor * cin[i][j] * cin[i][j];
		}
	}

	/* Free memory, close file and return */
	free_Carrayptrs(cin);
	free_Carrayptrs(cout);
	return PyArray_Return(matout);
}

/* A list of all the methods defined by this module. */
/* "iterate_point" is the name seen inside of Python */
/* "py_iterate_point" is the name of the C function handling the Python call */
/* "METH_VARGS" tells Python how to call the handler */
/* The {NULL, NULL} entry indicates the end of the method definitions */
static PyMethodDef mandelbrot_methods[] = { { "iterate_point", py_iterate_point,
		METH_VARARGS, iterate_point__doc__ }, { "test_mat", py_test_mat,
		METH_VARARGS, test_mat__doc__ }, { "add_prototype", py_add_prototype,
				METH_VARARGS, py_add_prototype__doc__ }, { "create_network", py_create_network,
		METH_VARARGS, py_create_network__doc__ }, { "delete_network",
		py_delete_network, METH_VARARGS, py_delete_network__doc__ },
		{ "train_network", py_train_network, METH_VARARGS,
				py_train_network__doc__ }, { "incremental_train_network",
				py_incremental_train_network, METH_VARARGS,
				py_incremental_train_network__doc__ }, { "load_network",
				py_load_network, METH_VARARGS, py_load_network__doc__ }, {
				"save_network", py_save_network, METH_VARARGS,
				py_save_network__doc__ }, { "process_network",
				py_process_network, METH_VARARGS, py_process_network__doc__ }, {
				"get_weights_network", py_get_weight_network, METH_VARARGS,
				py_get_weight_network__doc__ }, { "get_numofprototypes_network",
				py_get_numofprototypes_network, METH_VARARGS,
				py_get_numofprototypes_network__doc__ }, {
				"get_metricWeights_network", py_get_metricWeight_network, METH_VARARGS,
				py_get_metricWeight_network__doc__ }, { "get_eigenvectors_network",
				py_get_eingenvectors_network, METH_VARARGS,
				py_get_eingenvectors_network__doc__ }, {
				"get_prototypelabel_network", py_get_prototypelabel_network,
				METH_VARARGS, py_get_prototypelabel_network__doc__ }, {
				"set_learnrate_start_network", py_set_learnrate_start, METH_VARARGS,
				py_set_learnrate_start__doc__ }, {
				"set_learnrate_metricWeights_start_network", py_set_learnrate_metricWeights_start,
				METH_VARARGS, py_set_learnrate_metricWeights_start__doc__ }, { NULL, NULL } /* sentinel */
};

/* When Python imports a C module named 'X' it loads the module */
/* then looks for a method named "init"+X and calls it.  Hence */
/* for the module "mandelbrot" the initialization function is */
/* "initmandelbrot".  The PyMODINIT_FUNC helps with portability */
/* across operating systems and between C and C++ compilers */
PyMODINIT_FUNC initlibpythoninterface(void) {
	import_array()
		/* There have been several InitModule functions over time */
	PyObject* myclass = Py_InitModule3("libpythoninterface", mandelbrot_methods,
			mandelbrot__doc__);

	/*
	 * //constants:
	 const int NETTYPE_GMLVQ=Network::GMLVQ;
	 const int NETTYPE_GRLVQ=Network::GRLVQ;
	 const int NETTYPE_LGMLVQ=Network::LGMLVQ;
	 const int NETTYPE_LGRLVQ=Network::LGRLVQ;
	 const int NETTYPE_RLVQ=Network::GLVQ;

	 const int NETWORK_LEARNRATE=0;
	 const int NETWORK_LEARNRATE_PROTOTYPE=1;

	 *
	 */
	PyModule_AddIntConstant(myclass, "NETTYPE_GMLVQ", NETTYPE_GMLVQ);
	PyModule_AddIntConstant(myclass, "NETTYPE_GRLVQ", NETTYPE_GRLVQ);
	PyModule_AddIntConstant(myclass, "NETTYPE_LGMLVQ", NETTYPE_LGMLVQ);
	PyModule_AddIntConstant(myclass, "NETTYPE_LGRLVQ", NETTYPE_LGRLVQ);
	PyModule_AddIntConstant(myclass, "NETTYPE_GLVQ", NETTYPE_GLVQ);

	PyModule_AddIntConstant(myclass, "NETWORK_LEARNRATE", NETWORK_LEARNRATE);
	PyModule_AddIntConstant(myclass, "NETWORK_LEARNRATE_PROTOTYPE",
			NETWORK_LEARNRATE_PROTOTYPE);
}