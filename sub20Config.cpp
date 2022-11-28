#include <Python.h>

#include <mutex>

#include "aspCommon.hpp"

/*
  Python3 Compatiability
*/

#if PY_MAJOR_VERSION >= 3
    #define PyInt_FromLong PyLong_FromLong
    #define PyInt_AsLong PyLong_AsLong
    #define PyString_FromString PyUnicode_FromString
    #define PyString_AS_STRING PyBytes_AS_STRING
    
    #define MOD_ERROR_VAL NULL
    #define MOD_SUCCESS_VAL(val) val
    #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
    #define MOD_DEF(ob, name, methods, doc) \
       static struct PyModuleDef moduledef = { \
          PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
       ob = PyModule_Create(&moduledef);
#else
    #define MOD_ERROR_VAL
    #define MOD_SUCCESS_VAL(val)
    #define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
    #define MOD_DEF(ob, name, methods, doc) \
       ob = Py_InitModule3(name, methods, doc);
#endif


static PyMethodDef Sub20Methods[] = { {NULL, NULL,  0,  NULL} };


PyDoc_STRVAR(sub20config_doc, "Compile time configuration values used by the SUB-20 interface.");


MOD_INIT(sub20Config) {
  PyObject *m, *all, *value0, *value1;
  
  // Module definitions and functions
  MOD_DEF(m, "sub20Config", Sub20Methods, sub20config_doc);
  if( m == NULL ) {
      return MOD_ERROR_VAL;
  }
  
  // Constants
  value0 = PyInt_FromLong(MAX_BOARDS);
  PyModule_AddObject(m, "MAX_BOARDS", value0);
  value1 = PyInt_FromLong(STANDS_PER_BOARD);
  PyModule_AddObject(m, "STANDS_PER_BOARD", value1);
  
  // Module listings
  all = PyList_New(0);
  PyList_Append(all, PyString_FromString("MAX_BOARDS"));
  PyList_Append(all, PyString_FromString("STANDS_PER_BOARD"));
  PyModule_AddObject(m, "__all__", all);
  
  return MOD_SUCCESS_VAL(m);
}
  
