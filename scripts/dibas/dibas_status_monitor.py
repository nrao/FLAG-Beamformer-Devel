from dibas_utils import vegas_status, vegas_databuf
import curses, curses.wrapper
import time
import sys

def display_status(stdscr,stat,data):
    # Set non-blocking input
    stdscr.nodelay(1)
    run = 1

    # Look like gbtstatus (why not?)
    curses.init_pair(1, curses.COLOR_CYAN, curses.COLOR_BLACK)
    curses.init_pair(2, curses.COLOR_GREEN, curses.COLOR_BLACK)
    curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_RED)
    keycol = curses.color_pair(1)
    valcol = curses.color_pair(2)
    errcol = curses.color_pair(3)

    # display with fields sorted
    dosort=True
    if "-s" in sys.argv:
        dosort=False
    # no not filter out keywords begining with '_'
    dofilter=True
    if "-a" in sys.argv:
        dofilter=False

    # Loop 
    while (run):
        # Refresh status info
        stat.read()

        # Reset screen
        stdscr.erase()

        # Draw border
        stdscr.border()

        # Get dimensions
        (ymax,xmax) = stdscr.getmaxyx()

        # Display main status info
        onecol = False # Set True for one-column format
        col = 2
        curline = 0
        try:
            backend_type = stat["BACKEND"]
        except KeyError:
            backend_type = "VEGAS"
        stdscr.addstr(curline,col,"Current %s status:" % (backend_type), keycol);
        curline += 2
        flip=0
        #for k,v in stat.hdr.items():
        #titems=stat.hdr.items()
        titems=[]
        for k,v in stat.hdr.items():
            if len(k) < 1:
                continue
            if k[0] == "_" and not dofilter:
                titems.append((k,v))
            elif k[0] != '_':
                titems.append((k,v))
        if dosort:
            titems.sort()
        for k,v in titems:
            if len(k) < 1:
                continue
            if (curline < ymax-3):
                stdscr.addstr(curline,col,"%8s : "%k, keycol)
                stdscr.addstr("%s" % v, valcol)
            else:
                stdscr.addstr(ymax-3,col, "-- Increase window size --", errcol);
            if (flip or onecol):
                curline += 1
                col = 2
                flip = 0
            else:
                col = 40
                flip = 1
        col = 2
        if (flip and not onecol):
            curline += 1

        # Refresh current block info
        try:
            curblock = stat["CURBLOCK"]
        except KeyError:
            curblock=-1
        except IndexError:
            curblock=-1

        # Display current packet index, etc
        if (data is not None and curblock>=0 and curline < ymax-4):
            curline += 1
            stdscr.addstr(curline,col,"Current data block info:",keycol)
            curline += 1
            try:
                data.read_hdr(curblock)
                pktidx = data.hdr[curblock]["PKTIDX"]
            except IndexError:
                pktidx = "Unknown"
            except KeyError:
                pktidx = "Unknown"
            stdscr.addstr(curline,col,"%8s : " % "PKTIDX", keycol)
            stdscr.addstr("%s" % pktidx, valcol)

        # Figure out if we're folding
        foldmode = False
        curfold=1
        try:
            foldstat = stat["FOLDSTAT"]
            curfold = stat["CURFOLD"]
            if (foldstat!="exiting"):
                foldmode = True
        except:
            foldmode = False

        # Disable data buffer accesses
        foldmode = False
        
        # Display fold info
        if (foldmode and curline < ymax-4):
            try:
                folddata = vegas_databuf(2, stat.data_buffer_format())
            except:
                folddata = None
            curline += 2
            stdscr.addstr(curline,col,"Current fold block info:",keycol)
            curline += 1
            if folddata is not None:
                try:
                    folddata.read_hdr(curfold)
                    npkt = folddata.hdr[curfold]["NPKT"]
                    ndrop = folddata.hdr[curfold]["NDROP"]
                except IndexError:
                    npkt = "Unknown"
                    ndrop = "Unknown"
            else:
                npkt = "Unknown"
                ndrop = "Unknown"
            stdscr.addstr(curline,col,"%8s : " % "NPKT", keycol)
            stdscr.addstr("%s" % npkt, valcol)
            curline += 1
            stdscr.addstr(curline,col,"%8s : " % "NDROP", keycol)
            stdscr.addstr("%s" % ndrop, valcol)

        # Bottom info line
        stdscr.addstr(ymax-2,col,"Last update: " + time.asctime() \
                + "  -  Press 'q' to quit")

        # Redraw screen
        stdscr.refresh()

        # Sleep a bit
        time.sleep(0.25)

        # Look for input
        c = stdscr.getch()
        while (c != curses.ERR):
            if (c==ord('q')):
                run = 0
            c = stdscr.getch()

# Connect to vegas status, data bufs
g = vegas_status()
try:
    # Disable data buffer access
    d = None
    # d = vegas_databuf(1,g.data_buffer_format())
except:
    d = None

# Wrapper calls the main func
try:
    curses.wrapper(display_status,g,d)
except KeyboardInterrupt:
    print "Exiting..."


