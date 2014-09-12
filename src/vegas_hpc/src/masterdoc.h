
/** @mainpage

\section Important Items on the Page
<UL>
<LI> HPC Installation </LI>
<LI> HPC Process Context</LI>
<LI> HPC Interfaces </LI>
</UL>

\section Intro Introduction
The VEGAS and GUPPI HPC programs are of similar nature. Both utilize task level
parallelism to process data emitted by the roach devices and received by the HPC
host computer via a 10 GB Ethernet link.

Internally the vegas HPC program has various tasks which communicate through the use
of shared memory buffers. The configuration of the tasks is specified by a
shared memory segment, normally referred to as 'status memory'. Status memory
is used by the DIBAS and VEGAS scripts to write configuration values and
read status results. The format of the status memory area is an array of
80 column FITS keywords.

\section Process Process Context
The vegas_hpc_server, check_vegas_databuf and check_vegas_status are installed as suid
root, so that they may 'pin' the shared memory databuffers, and modify scheduler and
processor affinity attributes. However other privileged are not necessary, therefore
the vegas_hpc_server and vegasFitswriter will drop privileges and run as the
non-root user which originally started the process.

Each thread when started sets the thread priority and processor core affinity. This
allows a partitioning of the load of the various tasks at hand. This is especially
important on machines with multiple NUMA domains.


\section interfaces HPC Interfaces
The HPC program has five major interfaces: status memory for setup and status feedback;
data buffer memory for communicating between internal tasks and external processes such
as the vegasFitsWriter; a command fifo (or standard input) to initiate or stop observations;
a network port for receiving data produced by the Roach FPGA hardware, and output files
in FITS format.


\subsection confstatus Status Memory Keywords
Configuration keywords are mode specific. References which define the settings are:
<UL>
<LI><A href="../docs/vegas_memo_shmem.pdf">VEGAS Status Memory</A></LI>
<LI><A href="../docs/Vegas_hpc_critical_settings.pdf">Critical Settings for VEGAS</A></LI>
<LI><A href="../docs/vegas_hpc_dev_doc.pdf">HPC Developers Documentation</A></LI>
</UL>

\subsection conffitswriter Status Memory Keywords For the Vegas FITS Writer
This section describes the keywords in status memory that are used to convey information which 
is then written into the DIBAS spectral line FITS files. The file format is similar to the 
format described in the GBT vegas FITS specification. Differences are that if information 
necessary to complete a header-data unit (HDU) is not present in status memory, the table 
will not appear in the FITS output, or may have defaults filled in. Also keywords which are 
related to Ygor manager parameters or build-system are also omitted.

The names will begin with an underscore '_' to differentiate the keywords from the 'normal' status keywords.

Keyword Mappings
There are 4 tables or HDU's (omitting the main data table) which specify information. 
The status memory keyword encoding for tabular headers therefore will take the form:

_TXXX_NN

Where T is a character symbol identifying the table, XXX is a three character symbol which specifies 
the table column, and NN is a 2 digit numeric value which specifies the table row, starting at 01. 
A '_' shall separate the 3 character symbol and row number. For example, the state table entry for 
the 2nd row of the cal shall be '_SSREF_02. The names are intended to be somewhat cryptic to avoid 
clashes with status memory keywords which are unrelated to the FITS writer. 
The two character table encodings are:
<TABLE>
<TR><TH>FITS Table Name</TH><TH>Encoding with prefix</TH></TR>
<TR><TD>STATE</TD><TD>   _S </TD></TR>
<TR><TD>ACT_STATE</TD><TD>   _A </TD></TR>
<TR><TD>PORT </TD><TD>   _P </TD></TR>
<TR><TD>SAMPLER </TD><TD>    _M </TD></TR>
</TABLE>

The STATE table column encodings are:
<TABLE>
<TR><TH>STATE Table Column Name </TH><TH> Three Character Encoding </TH><TH> Table Column Combined Symbol </TH></TR>
<TR><TD>BLANKTIM </TD><TD> BLK </TD><TD>     _ SBLK_ </TD></TR>
<TR><TD>PHSESTRT </TD><TD>   PHS  </TD><TD>   _ SPHS_ </TD></TR>
<TR><TD>SIGREF </TD><TD> SRF  </TD><TD>   _ SSRF_ </TD></TR>
<TR><TD>CAL  </TD><TD>   CAL  </TD><TD>   _ SCAL_ </TD></TR>
</TABLE>

The ACT_STATE table column names are:
<TABLE>
<TR><TH>ACT_STATE Table Column Name  </TH><TH>   Three Character Encoding  </TH><TH>  Table Column Combined Symbol </TH></TR>
<TR><TD>ISIGREF1  </TD><TD>  ISA  </TD><TD>   _ AISA_ </TD></TR>
<TR><TD>ISIGREF2  </TD><TD>  ISB  </TD><TD>   _ AISB_ </TD></TR>
<TR><TD>ICAL  </TD><TD>  ICL  </TD><TD>   _ AICL_ </TD></TR>
<TR><TD>ESIGREF1  </TD><TD>  ESA  </TD><TD>   _ AESA_ </TD></TR>
<TR><TD>ESIGREF2 </TD><TD>   ESB  </TD><TD>   _ AESB_ </TD></TR>
<TR><TD>ECAL </TD><TD>   ECL  </TD><TD>   _ AECL_ </TD></TR>
</TABLE>

The SAMPLER table column name encodings are:
<TABLE>
<TR><TH>SAMPLER Table Column Name </TH><TH>  Three Character Encoding  </TH><TH>  Table Column Combined Symbol </TH></TR>
<TR><TD>BANK_A </TD><TD>  BKA   </TD><TD>   _ MBKA_ </TD></TR>
<TR><TD>PORT_A   </TD><TD>PTA   </TD><TD>   _ MPTA_ </TD></TR>
<TR><TD>BANK_B  </TD><TD> BKB    </TD><TD>  _ MBKB_ </TD></TR>
<TR><TD>PORT_B  </TD><TD> PTB    </TD><TD>  _ MPTB_ </TD></TR>
<TR><TD>DATATYPE  </TD><TD>   DTP   </TD><TD>   _ MDTP_ </TD></TR>
<TR><TD>SUBBAND </TD><TD>     SBD   </TD><TD>   _ MSBD_ </TD></TR>
<TR><TD>CRVAL1  </TD><TD> CR1   </TD><TD>   _ MCR1_ </TD></TR>
<TR><TD>CDELT1  </TD><TD> CDL   </TD><TD>   _ MCDL_ </TD></TR>
<TR><TD>FREQRES  </TD><TD>    FQR   </TD><TD>   _ MFQR_ </TD></TR>
</TABLE>

The PORT table column name encodings are:
<TABLE>
<TR><TH>PORT Table Column Name </TH><TH>  Three Character Encoding   </TH><TH>  Table Column Combined Symbol </TH></TR>
<TR><TD>BANK </TD><TD>    BNK   </TD><TD>   _ PBNK_ </TD></TR>
<TR><TD>PORT   </TD><TD>  PRT   </TD><TD>   _ PPRT_ </TD></TR>
<TR><TD>MEASPWR   </TD><TD>   PWR   </TD><TD>   _ PPWR_ </TD></TR>
<TR><TD>T_N_SW   </TD><TD>TNS    </TD><TD>  _ PTNS_ </TD></TR>
</TABLE>

For other scalar valued keywords, the keyword in shared memory is the same as the FIT keyword, except as noted. For definitions of each keyword see VEGAS FITS description in the link above.

    Primary header:
        OBJECT
        OBSID
        SCAN [SCANNUM in status mem]
        BANK [BANKNAM in status mem]
        NCHAN
        MODENUM
        BASE_BW
        NOISESRC 
    State Table
        NUMPHASE
        SWPERIOD
        SWMASTER 
    Sampler Table
        POLARIZE
        CRPIX1 [_MCRPIX1 in status mem] 
    DATA Table:
        SWPERINT
        UTCSTART
        UTDSTART
        DURATION ['EXPOSURE' in status mem] 




\subsection netsettings Network Settings


\subsection databufs Data Buffers

\subsection fitsformat FITS Files
The documentation of the FITS files output by vegas is available in
<A href="http://www.gb.nrao.edu/GBT/MC/doc/dataproc/gbtVEGASFits/gbtVEGASFits.pdf">the VEGAS
FITS Specification.</A>



\section installing Installation
The following is a step-by-step procedure to install the VEGAS HPC software.
<UL>
<LI> Set the environment variable VEGAS to the root of the vegas_devel directory e.g:<I>export VEGAS=/usr/src/vegas_devel ; cd $VEGAS</I> </LI>
<LI> change directory into the $VEGAS/src/vegas_hpc directory.</LI>
<LI> Edit the vegas_spec-hpc.bash script and set the following environment variables need to be set to the correct locations:
<UL>
<LI> VEGAS_DIR=$VEGAS/vegas_hpc</LI>
<LI> CUDA= (the cuda development directory: e.g: /opt/local/cuda50)</LI>
<LI> PYSLALIB= (the python Starlink SLA library e.g: /usr/lib/python2.7/site-packages/pyslalib) </LI>
<LI> </LI>
</UL>

</UL>




\section Useful References

Some useful references:
<UL>
<LI> <A href="https://casper.berkeley.edu/wiki/images/1/1a/Vegas_hpc_dev_doc.pdf">
Vegas HPC Developer Documentation</A></LI>
<LI> <A href="https://casper.berkeley.edu/wiki">VEGAS High Bandwidth Packet Format</A> </LI>
<LI> <A href="https://casper.berkeley.edu/wiki">VEGAS Low Bandwidth Packet Format</A> </LI>
<LI> <A href="../doc/">Paul Demorest's HPC presentation slides</A></LI>
</UL>



*/

