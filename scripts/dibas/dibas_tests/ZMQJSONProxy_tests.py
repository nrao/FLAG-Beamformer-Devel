######################################################################
#
#  ZMQJSONProxy_tests.py -- Unit tests for the ZMQJSONProxy classes.
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

import threading
import zmq
from ZMQJSONProxy import ZMQJSONProxyServer
from ZMQJSONProxy import ZMQJSONProxyClient
from time import sleep
from nose import with_setup

class Foo:
    def cat(self):
        """
        Cat process.
        """
        return "meow"

    def dog(self):
        """
        Dog process.
        """
        return "woof"

    def frog(self):
        """
        Frog process.
        """
        return "rivet!"

    def add_two(self, x, y):
        """
        Adds two values together.
        """
        return x + y


url = "inproc://beautiful_unique_sparklepony"
proxy = None
foo = None
ctx = zmq.Context()

def setup_zmq_server():
    global proxy
    global foo

    proxy = ZMQJSONProxyServer(ctx, url)
    foo = Foo()
    proxy.expose("foo", foo)

    def threadfunc():
        proxy.run_loop()

    # Run the proxy in a separate thread. TBF: Caution! exceptions here
    # leave this running, because the 'proxy.quit_loop()' call is
    # skipped. Thus the unit test will hang.  This is why asserts come
    # after 'proxy.quit_loop()'.
    threading.Thread(target=threadfunc).start()

def stop_zmq_server():
    global proxy
    global foo
    proxy.quit_loop()
    del proxy
    del foo
    foo = None
    proxy = None

@with_setup(setup_zmq_server, stop_zmq_server)
def test_ZMQ_Proxy_Interface():

    # Now for a few tests. We're not testing the proxy client, just the
    # proxy server, so we'll create a ZMQ socket and feed it the
    # expected dictionaries, and examine the replies.

    # Test for all functions of 'Foo':
    test_sock = ctx.socket(zmq.REQ)
    test_sock.connect(url)
    msg = {"name": "foo", "proc": "cat", "args": [], "kwargs": {}}
    test_sock.send_json(msg)
    cat_ret = test_sock.recv_json()

    msg = {"name": "foo", "proc": "dog", "args": [], "kwargs": {}}
    test_sock.send_json(msg)
    dog_ret = test_sock.recv_json()

    msg = {"name": "foo", "proc": "frog", "args": [], "kwargs": {}}
    test_sock.send_json(msg)
    frog_ret = test_sock.recv_json()

    # Test to ensure 'list_methods' works. Should return method names,
    # and doc strings:
    msg = {"name": "foo", "proc": "list_methods", "args": [], "kwargs": {}}
    test_sock.send_json(msg)
    list_ret = test_sock.recv_json()

    # Test exception handling; in this case, we're asking for a function
    # in 'bar', which doesn't exist.
    msg = {"name": "bar", "proc": "cat", "args": [], "kwargs": {}}
    test_sock.send_json(msg)
    except_ret = test_sock.recv_json()

    # Test positional arguments. 'add_two' expects two arguments.
    msg = {"name": "foo", "proc": "add_two", "args": [2, 2], "kwargs": {}}
    test_sock.send_json(msg)
    add_listargs_ret = test_sock.recv_json()

    # Test keyword arguments. 'add_two' expects two arguments, 'x', 'y'.
    msg = {"name": "foo", "proc": "add_two", "args": [], "kwargs": {"x": 2, "y": 3}}
    test_sock.send_json(msg)
    add_kwargs_ret = test_sock.recv_json()

    # Test mixed args: first is bound positionally, second is keyword.
    msg = {"name": "foo", "proc": "add_two", "args": [3], "kwargs": {"y": 3}}
    test_sock.send_json(msg)
    add_mixedargs_ret = test_sock.recv_json()

    assert cat_ret == "meow"
    assert dog_ret == "woof"
    assert frog_ret == "rivet!"

    list_ret.sort() # Ensure they are in the order test thinks they are
    assert len(list_ret) == 4 # 4 functions: cat, dog, frog, add_two
    # test for function name & doc string
    assert 'add_two' in list_ret[0]
    assert 'Adds two' in list_ret[0][1]
    assert 'cat' in list_ret[1]
    assert 'Cat process' in list_ret[1][1]
    assert 'dog' in list_ret[2]
    assert 'Dog process' in list_ret[2][1]
    assert 'frog' in list_ret[3]
    assert 'Frog process' in list_ret[3][1]
    assert "KeyError" in except_ret
    # test for use of params. Result '4' is from two positional args,
    # '5' from two keyword args, and '6' from a positional and a keyword
    # arg used together.
    assert add_listargs_ret == 4
    assert add_kwargs_ret == 5
    assert add_mixedargs_ret == 6

@with_setup(setup_zmq_server, stop_zmq_server)
def test_ZMQ_proxy_client():
    """
    Checks the client proxy class. The client proxy should obtain the
    exposed function names of the named object on the server, the doc
    strings for each of those, and should be able to call the functions
    as if they were local.
    """

    foo_proxy = ZMQJSONProxyClient(ctx, 'foo', url)

    assert 'Cat process' in foo_proxy.cat.__doc__
    cat_ret = foo_proxy.cat()
    assert cat_ret == "meow"

    assert 'Dog process' in foo_proxy.dog.__doc__
    dog_ret = foo_proxy.dog()
    assert dog_ret == "woof"

    assert 'Frog process' in foo_proxy.frog.__doc__
    frog_ret = foo_proxy.frog()
    assert frog_ret == "rivet!"

    assert 'Adds two' in foo_proxy.add_two.__doc__
    add_ret = foo_proxy.add_two(2, 2)
    assert add_ret == 4
    add_ret = foo_proxy.add_two(2, y = 3)
    assert add_ret == 5
    add_ret = foo_proxy.add_two(y = 3, x = 4)
    assert add_ret == 7
