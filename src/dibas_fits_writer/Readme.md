# Introduction

Despite the name, this directory is responsible for the FLAG-Beamformer FITS writer. 

## Usage

Usage: bfFitsWriter (options)
Options:
  -t , --test          run a test
  -m , --mode          'c' for Cov. Matrix, 'p' for Pulsar
  -i n, --instance=nn  instance id

The main executable is designed to work both in online, real-time with shared memory buffer, and also in various offline modes.  It also handles both Covariance Matrix and Pulsar data modes.

## FITS Writers

The FITS writer class heirarchy looks like this, where FitsIO has been stolen from GBT M&C's ygor repository:

      FitsIO
        ^
        |
      BfFitsIO
      ^      ^
      |      |
BfCovFitsIO BfPulsarFitsIO

The final two sub-classes handle the difference between the Cov. Matrix and Puslar data modes. 

## Online/Offline Modes

### ONLINE MODE:

These FITS writers are used in online mode by the BfFitsThread class, which handles the interactions with shared memory buffers.

BfFitsThread
   *
   |
BfFitsIO

### OFFLINE MODES:

When the executable is also passed the '-t' option, various offline operations can be run.  Changing what is run per mode must be hard-coded in mainTest.cc, for now.

#### OFFLINE Cov Matrix:

   * mainTestCov: very basic test of the BfCovFits class.  TBF: this exposes a bug in the matrix ordering.
   * fishFits2CovFitsTest: this reads a FITS file in 'fishfits' format, from the Jan. 2015 PAF commissioning data.  It then attempts to parse this data and place them correctly in 8 different Cov. Matrix FITS files using BfCovFitsIO.  TBF.

#### OFFLINE Pulsar:

   * mainTestPulsar: uses the FakePulsarToFits class convert fake pulsar data to a number of FITS files using BfPulsarFitsIO.  TBF.

This class uses the BfPulsarFitsIO FITS writer, and another class for parsking the source of the fake pulsar data.

    FakePulsarToFits
     *             *     
     |             |     
BfPulsarFitsIO    FakePulsarFile

## Scripts

There are a number of proof-of-concept scripts that were developed in order to aid the development of some of these classes.

   * gpu2fitsOrderComplex.py: As the name implies, this does the job of converting the data as it's written out by the GPU (disordered, bloated) into the order needed by the FITS writer (ordered, normalized).
   * matrix.py: This appears to be an original attempt to do what gpu2fitsOrderComplex is doing.
   * pulsarData.py: Demonstrates how to read data from the associated smallFakePulsar.ascii, and put that data correctly into a collection of FITS files.  Includes asserts to make sure this is actually working.

