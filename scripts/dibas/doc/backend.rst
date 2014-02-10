Backend
=======

.. toctree::
   :maxdepth: 1

The DIBAS is a multi-mode instrument. Modes are selected by programming the FPGA with new BOF files and initializing them according to the needs for that mode.

Each BOF file has its own set of registers which do not necessarily do the same things and are not named the same way accross BOF files. The Backend classes exist to abstract away the differences and provide a common interface to all the BOF files.

The Bank class uses a strategy pattern [strategy]_ to deal with each different BOF file. When a mode is selected an object that derives from Backend is constructed and handles all requests to the Backend functions. Each Backend class implements the Backend interface as appropriate.  There are currently 8 backend classes:

   * **Backend:** the base class
   * **GuppiBackend:** incoherent mode pulsar backend based on GBT GUPPI instrument
   * **GuppiCODDBackend:** Coherent de-dispersion mode pulsar backend, based on GBT GUPPI instrument.
   * **VegasBackend:** Base class for the spectral line modes, based on GBT VEGAS spectrometer
   * **VegasHBWBackend:** Instance class for the wide band spectral line modes (modes 1-3)
   * **VegasLBWBackend:** Base class for the narrow band spectral line modes (modes 4-29)
   * **VegasL1LBWBackend:** Instance class for the L1 lbw modes. These modes provide a fixed narrow-band window.
   * **VegasL8LBWBackend:** Instance class for the L8 lbw modes. These modes provide 1 or 8 movable narrow-band windows.

The base class provides the interface and a fair amount of common functionality (gettting/setting status memory, roach registers, etc.) The individual derived backend classes provide the functionality that must be specific to each backend.

.. toctree::
   :maxdepth: 1

class Backend
-------------

.. automodule:: Backend
.. autoclass:: Backend.Backend
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

class VegasBackend
------------------

.. automodule:: VegasBackend
.. autoclass:: VegasBackend.VegasBackend
   :members:
   :private-members:

class VegasHBWBackend
---------------------

.. automodule:: VegasHBWBackend
.. autoclass:: VegasHBWBackend.VegasHBWBackend
   :members:
   :private-members:

class VegasLBWBackend
---------------------

.. automodule:: VegasLBWBackend
.. autoclass:: VegasLBWBackend.VegasLBWBackend
   :members:
   :private-members:

class VegasL1LBWBackend
-----------------------

.. automodule:: VegasL1LBWBackend
.. autoclass:: VegasL1LBWBackend.VegasL1LBWBackend
   :members:
   :private-members:

class VegasL8LBWBackend
-----------------------

.. automodule:: VegasL8LBWBackend
.. autoclass:: VegasL8LBWBackend.VegasL8LBWBackend
   :members:
   :private-members:

.. [strategy] *Design Patterns* by Erich Gamma et al.
