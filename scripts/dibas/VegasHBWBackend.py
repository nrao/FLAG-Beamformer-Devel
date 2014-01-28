######################################################################
#
#  VegasHBWBackend.py -- Instance class for the DIBAS Vegas
#  modes. Derives Vegas mode functionality from VegasBackend, adding
#  only HBW specific functionality, and I/O with the roach and with the
#  status shared memory.
#
#  Copyright (C) 2014 Associated Universities, Inc. Washington DC, USA.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Correspondence concerning GBT software should be addressed as follows:
#  GBT Operations
#  National Radio Astronomy Observatory
#  P. O. Box 2
#  Green Bank, WV 24944-0002 USA
#
######################################################################

from VegasBackend import VegasBackend

class VegasHBWBackend(VegasBackend):
    """A class which implements the VEGAS HBW mode specific functionality,
    and which communicates with the roach and with the HPC programs via
    shared memory.

    VegasHBWBackend(theBank, theMode, theRoach = None, theValon = None)

    Where:

    * *theBank:* Instance of specific bank configuration data BankData.
    * *theMode:* Instance of specific mode configuration data ModeData.
    * *theRoach:* Instance of katcp_wrapper
    * *theValon:* instance of ValonKATCP
    * *unit_test:* Set to true to unit test. Will not attempt to talk to
      roach, shared memory, etc.

    """

    def __init__(self, theBank, theMode, theRoach, theValon, hpc_macs, unit_test = False):
        """
        Creates an instance of VegasHBWBackend
        """

        VegasBackend.__init__(self, theBank, theMode, theRoach, theValon, hpc_macs, unit_test)

        self.prepare()
        self.start_hpc()
        self.start_fits_writer()

    def __del__(self):
        """
        Perform some cleanup tasks.
        """
        if self.fits_writer_process is not None:
            print "Deleting FITS writer!"
            self.stop_fits_writer()

    def cleanup(self):
        """
        This explicitly cleans up any child processes. This will be called
        by the player before deleting the backend object.
        """
        print "VegasBackend: cleaning up hpc and fits writer."
        self.stop_hpc()
        self.stop_fits_writer()

    # prepare() for this class calls the base class prepare then does
    # the bare minimum required just for this backend, and then writes
    # to hardware, HPC, shared memory, etc.
    def prepare(self):
        """
        This command writes calculated values to the hardware and status memory.
        This command should be run prior to the first scan to properly setup
        the hardware.

        The sequence of commands to set up a measurement is thus typically::

          be.set_param(...)
          be.set_param(...)
          ...
          be.set_param(...)
          be.prepare()
        """

        super(VegasHBWBackend, self).prepare()

        self.set_register(acc_len=self.acc_len)

        # Talk to outside things: status memory, HPC programs, roach

        if self.bank is not None:
            self.write_status(**self.status_mem_local)
        else:
            for i in self.status_mem_local.keys():
                print "%s = %s" % (i, self.status_mem_local[i])

        if self.roach:
            self.write_registers(**self.roach_registers_local)


    def _sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler for HBW mode
        """
        self.sampler_frequency = self.frequency * 1e6 * 2

    # _set_state_table_keywords() overrides the parent version, but
    # should call the parent version first thing, as it will build on
    # what the parent function does. Since the parent class prepare()
    # calls this, no need to call this from this Backend's prepare.
    def _set_state_table_keywords(self):
        """
        Gather status sets here
        Not yet sure what to place here...
        """

        super(VegasHBWBackend, self)._set_state_table_keywords()
        self.set_status(BW_MODE = "high")
        self.set_status(OBS_MODE = "HBW")
