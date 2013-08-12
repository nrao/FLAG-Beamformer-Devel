ConfigData
==========

.. toctree::
   :maxdepth: 1

The *ConfigData* family of classes read the configuration data from the configuration file and store the values. There are two classes based on *ConfigData*:

   * *BankData*
   * *ModeData*

*BankData* stores values read from a specific *BANK* section of the configuration file (bank dependent), and serve to configure each individual bank, with bank-specific data such as IP addresses, etc. For example: if a Player is controlling Bank A, it will read and record the configuration data from section [BANKA] in the configuration file.

*ModeData* stores values read from the *MODE* sections of the configuration file. All modes are used by all backends, depending on instrument configuration.

class ConfigData
----------------

.. automodule:: ConfigData
.. autoclass:: ConfigData.ConfigData
   :members:
   :private-members:

class BankData
----------------

.. autoclass:: ConfigData.BankData
   :members:
   :private-members:


class ModeData
----------------

.. autoclass:: ConfigData.ModeData
   :members:
   :private-members:
