######################################################################
#
#  dealer.py -- Bank controller.  One Dealer, many Players.
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

import types
import zmq

def datetime_to_tuple(dt):
    return (dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.microsecond)

class ZMQJSONProxy(object):
    def __init__(self, ctx, obj_name, url):
        self._url = url
        self._obj_name = obj_name
        self._sock = ctx.socket(zmq.REQ)
        self._sock.connect(self._url)
        self._poller = zmq.Poller()
        self._poller.register(self._sock, zmq.POLLIN)
        self._sock.send_json({'proc': 'list_methods', 'args': [], 'kwargs': {}})
        methods = self._sock.recv_json()
        my_methods = [m for m in methods if self._obj_name in m[0]]
        for m, d in my_methods:
            self._add_method(m, d)

    def _add_method(self, method_name, doc_string):
        mn = method_name.split('.')[1] # we want the 'bar' part of 'foo.bar'
        method = types.MethodType(self._generate_method(mn), self)
        method.__func__.__doc__ = doc_string
        self.__dict__[mn]=method

    def _generate_method(self, name):
        def new_method(self, *args, **kwargs):
            return self._do_the_deed(name, *args, **kwargs)
        return new_method

    def _do_the_deed(self, *args, **kwargs):
        msg = {'proc': self._obj_name + '.' + args[0], 'args': args[1:], 'kwargs': kwargs}
        self._sock.send_json(msg)
        socks = dict(self._poller.poll(10000))

        if self._sock in socks and socks[self._sock] == zmq.POLLIN:
            repl = self._sock.recv_json()
            return repl
        else:
            print "socket timed out!"
            return None

ctx = zmq.Context()
bp = ZMQJSONProxy(ctx, 'bank', 'tcp://north:6667')
bp.katcp = ZMQJSONProxy(ctx, 'katcp', 'tcp://north:6667')
