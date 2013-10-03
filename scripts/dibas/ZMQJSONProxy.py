######################################################################
#
#  ZMQJSONProxy.py -- Implements proxy classes to enable an object to be
#  proxied via ZMQ and JSON. Currently only methods are proxied. Methods
#  that are exported may be called on the proxy class as if they were
#  local. They even include the original doc strings, and may be called
#  using positional or keyword arguments. Typical use (see unit tests):
#
#  class Foo:
#      def add_two(self, x, y):
#          """
#          Adds two values together.
#          """
#          return x + y
#
#  On server, assuming a zmq.Context() 'ctx' and a well known url:
#
#      proxy = ZMQJSONProxyServer(ctx, url)
#      foo = Foo()
#      proxy.expose("foo", foo)
#      proxy.run_loop()
#
#  On the client (same assumptions):
#
#      foo_proxy = ZMQJSONProxyClient(ctx, 'foo', url)
#      ret = foo_proxy.add_two(2, 2) # ret = 4
#      ret = foo_proxy.add_two(2, y = 3) # ret = 5
#      ret = foo_proxy.add_two(y = 3, x = 4) #ret = 7
#      foo_proxy.add_two.__doc__ # returns Foo.add_two's doc string
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

import zmq
import sys
import traceback
import types
import inspect
import thread
import time

class ZMQJSONProxyServer(object):

    def __init__(self, ctx, URL):
        """
        Initializes the proxy server. Binds the server to a url, but
        does not run the server. Use member 'run_loop()' to do that.

        ctx: A ZeroMQ context (zmq.Context())
        URL: The server's URL. May be any legal 0MQ URL, 'inproc', 'IPC', 'tcp'.
        """
        self.url = URL
        self.interfaces = {}
        self.ctx = ctx
        self.s = self.ctx.socket(zmq.REP)
        self.pipe = self.ctx.socket(zmq.PULL)
        self.s.bind(URL)
        self.pipe_url = "inproc://ctrl_pipe"
        self.pipe.bind(self.pipe_url)
        self.exit_flag = False


    def expose(self, name, obj):
        """
        expose(name, obj):
          name: The name of the exposed interface, may be anything.
          obj: the object whose functions should be proxied by this class.

        This function collects the named functions of 'obj' and saves
        the names and functions in a dictionary, which itself is an
        element of the 'self.interfaces' dictionary under 'name', thus
        allowing any number of objects to be proxied.
        """
        self.interfaces[name] = \
            {p[0]: p[1] for p in inspect.getmembers(obj, predicate=inspect.ismethod)}

    def dispatch(self, message):
        """
        Given a dictionary 'message', dispatches it, calling the correct
        function.  The message must have the following keys:

        'name'   : The name of the object being proxied.
        'proc'   : The name of the member function for that object
        'args'   : The list of arguments for that member function
        'kwargs' : The list of keyword arguments for that member function.
        """
        try:
            f_dict = self.interfaces[message['name']]
            proc = f_dict[str(message['proc'])]
            args = message['args']
            kwargs = message['kwargs']
            return proc(*args, **kwargs)
        except:
            return self.formatExceptionInfo(10)

    def formatExceptionInfo(self, maxTBlevel=5):
        """
        Obtains information from the last exception thrown and extracts
        the exception name, data and traceback, returning them in a tuple
        (string, string, [string, string, ...]).  The traceback is a list
        which will be 'maxTBlevel' deep.
        """
        cla, exc, trbk = sys.exc_info()
        excName = cla.__name__
        excArgs = exc.__str__()
        excTb = traceback.format_tb(trbk, maxTBlevel)
        return (excName, excArgs, excTb)

    def list_proxied_interfaces(self, name):
        """
        List all the exported functions of interface 'name'.
        """
        f_dict = self.interfaces[name]
        exported_funcs = \
            [(ef, f_dict[ef].__doc__) \
                 for ef in filter(lambda x:x[0] != '_', f_dict.keys())]

        if self.s:
            self.s.send_json(exported_funcs)

    def run_loop(self, watchdogfn = None):
        """
        Runs the server.  This may be run in the server's main thread,
        or can easily be run in another thread. It sets up a poller that
        consists of 2 sockets: the server socket that processes messages
        from the remote proxy client, and a pipe socket that controls
        this loop. When the pipe receives the 'QUIT' message, the loop
        will exit.
        """
        done = False
        poller = zmq.Poller()
        poller.register(self.s, zmq.POLLIN)
        poller.register(self.pipe, zmq.POLLIN)

        if watchdogfn:
            try:
                thread.start_new_thread( self.generate_watchdog_messages, ("WATCHDOG", 1, ) )
            except:
                print "Error: unable to start watchdog thread. There will be no watchdog."

        while not done:
            try:
                socks = dict(poller.poll(120000))

                if self.s in socks and socks[self.s] == zmq.POLLIN:
                    message = self.s.recv_json()

                    if message['proc'] == 'list_methods':
                        self.list_proxied_interfaces(message['name'])
                    else:
                        ret_msg = self.dispatch(message)
                        self.s.send_json(ret_msg)

                if self.pipe in socks and socks[self.pipe] == zmq.POLLIN:
                    message = self.pipe.recv_json()

                    if message == "QUIT":
                        done = True

                    if message == "WATCHDOG":
                        # This message should never come if watchdogfn
                        # is None, but check to make sure anyway.
                        if watchdogfn:
                            watchdogfn()

            except zmq.core.ZMQError as e:
                print "zmq.core.ZMQError:", str(e)

        self.exit_flag = True

    def quit_loop(self):
        """
        Tells the main loop 'run_loop()' to exit by passing it a message
        on its control pipe socket.
        """
        pc = self.ctx.socket(zmq.PUSH)
        pc.connect(self.pipe_url)
        pc.send_json("QUIT")

    def generate_watchdog_messages(self, name, delay):
        """Runs as a separate thread, generates 'WATCHDOG' messages for the
        main loop.

        """
        pc = self.ctx.socket(zmq.PUSH)
        pc.connect(self.pipe_url)

        while not self.exit_flag:
            pc.send_json(name)
            time.sleep(delay)



class ZMQJSONProxyClient(object):
    """
    A proxy class to proxy remote objects over a ZMQ connection using
    the JSON protocol. Currently only proxies member functions. Also,
    care must be taken to ensure any parameters or return types can be
    JSON serialized & deserialized, and that the resulting object can be
    reconstructed on the receiving end. Plain Old Types (int, float,
    etc.) and python types (dict, list, set, tuple) present no problem.
    """
    def __init__(self, ctx, obj_name, url):
        """
        Initializes a proxy client.

        ctx      : The 0MQ context (zmq.Context())
        obj_name : The name of the object exposed on the server
        url      : The server's url
        """
        self._url = url
        self._obj_name = obj_name
        self._sock = ctx.socket(zmq.REQ)
        self._sock.connect(self._url)
        self._poller = zmq.Poller()
        self._poller.register(self._sock, zmq.POLLIN)
        self._sock.send_json({'name': self._obj_name, 'proc': 'list_methods', 'args': [], 'kwargs': {}})
        methods = self._sock.recv_json()

        for m, d in methods:
            self._add_method(m, d)

    def _add_method(self, method_name, doc_string):
        """
        Adds a proxy method to the client that will behave just as the
        corresponding method on the server does.
        """
        method = types.MethodType(self._generate_method(method_name), self)
        method.__func__.__doc__ = doc_string
        self.__dict__[method_name]=method

    def _generate_method(self, name):
        """
        Creates a closure that will make a remote call to the
        appropriate remote method.
        """
        def new_method(self, *args, **kwargs):
            return self._do_the_deed(name, *args, **kwargs)
        return new_method

    def _do_the_deed(self, *args, **kwargs):
        """
        This method handles the nuts and bolts of calling the remote
        function. It does this by constructing a dictionary that the
        remote server can use to call the correct function and pass on
        the arguments to it, and handles the return value or exception
        information.
        """
        msg = {'name': self._obj_name, 'proc': args[0], 'args': args[1:], 'kwargs': kwargs}
        self._sock.send_json(msg)
        # TBF: 8 minutes for a reply. Need to handle case where it times out.
        socks = dict(self._poller.poll(480000))

        if self._sock in socks and socks[self._sock] == zmq.POLLIN:
            repl = self._sock.recv_json()
            return repl
        else:
            # TBF: FIX THIS! Need to close out the socket, and reconnect
            # to the service if this happens.
            print "socket timed out!"
            return None
