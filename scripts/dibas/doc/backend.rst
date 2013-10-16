Backend
=======

.. toctree::
   :maxdepth: 1

The DIBAS is a multi-mode instrument. Modes are selected by programming the FPGA with new BOF files and initializing them according to the needs for that mode.

Each BOF file has its own set of registers which do not necessarily do the same things and are not named the same way accross BOF files. The Backend classes exist to abstract away the differences and provide a common interface to all the BOF files.

The Bank class uses a strategy pattern [strategy]_ to deal with each different BOF file. When a mode is selected an object that derives from Backend is constructed and handles all requests to the Backend functions. Each Backend class implements the Backend interface as appropriate.  There are currently 4 backend classes:

   * **Backend:** the base class
   * **VegasBackend:** provides spectral line modes, based on GBT VEGAS spectrometer
   * **GuppiBackend:** incoherent mode pulsar backend based on GBT GUPPI instrument
   * **GuppiCODDBackend:** Coherent de-dispersion mode pulsar backend, based on GBT GUPPI instrument.

The base class provides the interface and a fair amount of common functionality (gettting/setting status memory, roach registers, etc.) The individual derived backend classes provide the functionality that must be specific to each backend.

.. toctree::
   :maxdepth: 1

class Backend
-------------

.. automodule:: Backend
.. autoclass:: Backend.Backend
   :members:
   :private-members:

class VegasBackend
------------------

.. automodule:: VegasBackend
.. autoclass:: VegasBackend.VegasBackend
   :members:
   :private-members:

class GuppiBackend
------------------

.. automodule:: GuppiBackend
.. autoclass:: GuppiBackend.GuppiBackend
   :members:
   :private-members:

class GuppiCODDBackend
----------------------

.. automodule:: GuppiCODDBackend
.. autoclass:: GuppiCODDBackend.GuppiCODDBackend
   :members:
   :private-members:

.. [strategy] *Design Patterns* by Erich Gamma et al.
