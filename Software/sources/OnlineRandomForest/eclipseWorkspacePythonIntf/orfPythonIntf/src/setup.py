 
from distutils.core import setup, Extension

module1 = Extension('liborfPythonIntf',
                    include_dirs = ['/usr/include/python2.7',
                    '/usr/include/python2.7/numpy',
                    '/home/viktor/svn/viktlosi/Software/sources/OnlineRandomForest/eclipseWorkspacePythonIntf/orfAdaptedToPythonIntf/src',
                    '/home/viktor/svn/viktlosi/Software/sources/OnlineRandomForest/eigen/install/include/eigen2'],
                    libraries = ['orfAdaptedToPythonIntf'],
                    library_dirs = ['/home/viktor/svn/viktlosi/Software/sources/OnlineRandomForest/eclipseWorkspacePythonIntf/orfAdaptedToPythonIntf'],
                    sources = ['intf.cpp'])

setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a demo package',
       author = 'Martin v. Loewis',
       author_email = 'martin@v.loewis.de',
       url = 'http://docs.python.org/extending/building',
       long_description = '''This is really just a demo package.''',
       ext_modules = [module1])