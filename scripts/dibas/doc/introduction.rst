Introduction
============

The DIBAS spectrometer control software is a distributed system written in Python that consists of several backend servers, each running on an HPC computer, designated as *Players*, controlled and coordinated by a simple client library called the *Dealer* using a ZeroMQ RPC mechanism with JSON encoding.

.. figure:: dealer-player.png
   :scale: 75 %
   :alt: Block diagram of Dealer and Players

   *Figure 1: Block diagram of Dealer and Players*

Players may either be run interactively (useful for debugging), or be run as daemons and may remain running indefinitely. Players must know a great deal about the DIBAS configuration, and must have access to the needed VEGAS GUPPI libraries and DAQ programs, in addition to 3d party modules.

The Dealer library is much more lightweight. The Dealer may either be run interactively, or imported into scripts or the telescope control system as needed. A Dealer only needs to know the URL(s) of one or more player(s), and will obtain all the information they need from those Players. In addition the Python installation for a Dealer needs only one specialized library, PyZMQ.

Dealers may come and go as needed. Further, there may be more than one Dealer running at any given time, each with a designated function. For example a status display program may run the Dealer interface to obtain status information for each Player and display it in one convenient location. Another Dealer may be included in the telescope control system and may be tasked with starting and stopping scans under telescope control. Yet another Dealer may be used in a user script to set up observations.
