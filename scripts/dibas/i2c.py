######################################################################
#
#  valon_katcp.py -- Valon over KATCP support. Duck typed after the
#  Synthesizer object in valon_synth.
#
#  Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
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

import struct

class I2CException(Exception):
   def __init__(self, message):
        Exception.__init__(self, message)

class I2C(object):
    """
    An I2C clas.

    I2C(r)

    * *r:* corr.katcp_wrapper.FpgaClient object. This object is used for
      I2C communications to the ROACH. It is expected to be
      initialized.
    """

    def __init__(self, r):
        """
        __init__(self, r)

        Initializes the I2C class.
        """
        self.roach = r
        """The KATCP client object"""
        self.timeout = 5
        """The default timeout for katcp requests for this module"""

    def getI2CValue(self, addr, nbytes):
        """
        getI2CValue(addr, nbytes, data):

        * *addr:* The I2C address
        * *nbytes:* the number of bytes to get

        Returns the IF bits used to set the input filter.

        Example::

          bits = self.getI2CValue(0x38, 1)
        """
        
        if self.roach:
           # for katcp 0.3.5+
           # reply, informs = self.roach._request('i2c-read', self.timeout,
           #                                      addr, nbytes)
           reply, informs = self.roach._request('i2c-read',
                                                 addr, nbytes)
           v = reply.arguments[2]
           return (reply.arguments[0] == 'ok',
                   struct.unpack('>%iB' % nbytes, v))
        return (True, 0)



    def setI2CValue(self, addr, nbytes, data):
        """
        setI2CValue(addr, nbytes, data):

        * *addr:* the I2C address
        * *nbytes:* the number of bytes to send
        * *data:* the data to send

        Sets the IF bits used to set the input filter.

        Example::

          self.setI2CValue(0x38, 1, 0x25)
        """

        if self.roach:
           # for katcp 0.3.5+
           # reply, informs = self.roach._request(
           #     'i2c-write', self.timeout,
           #     addr, nbytes, struct.pack('>%iB' % nbytes, data))
           reply, informs = self.roach._request(
              'i2c-write',
              addr, nbytes, struct.pack('>%iB' % nbytes, data))
           return reply.arguments[0] == 'ok'
        return True
