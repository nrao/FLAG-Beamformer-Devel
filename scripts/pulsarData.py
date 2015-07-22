from copy import copy

def readAsciiFakePulsar(filename):
    data = []
    f = open(filename, 'r')
    ls = f.readlines()
    # each line is a sample
    for l in ls:
        # convert string to list of string numbers
        xs = [x for x in l.split(' ') if x != '' and x != ' ']
        # get rid of trailing \n
        xs[-1] = xs[-1][:-1]
        # convert to float
        xf = [float(x) for x in xs if x != '' and x != ' ']
        data.append(xf)
        
    f.close()
    return data

def getFakePulsarDataFromFiles(numBeams, numChans):
    #fn = '/users/arane/fakepsr1.ascii'
    fn = './smallFakePulsar.ascii'
    files = [fn]*numBeams # TBF
    pulsarData = []
    beamData = []
    for b in range(numBeams):
        f = files[b]
        beamData.append(readAsciiFakePulsar(f))
    # TBF: all the same sample length?
    numSamples = len(beamData[0])
    # TBF: all the same channels?
    numChansData = len(beamData[0][0])

    assert numChansData < numChans # TBF
    dupD = (numChans / numChansData) - 1
    addD = numChans % numChansData
    print numChansData, ' vs ', numChans, dupD, addD

    # go back and force the data to the right length
    for b in range(numBeams):
        for s in range(numSamples):
            origData = copy(beamData[b][s])
            for d in range(dupD):
                beamData[b][s].extend(origData)
            if addD > 0:
                beamData[b][s].extend(origData[:addD])
            print numChans, ' vs ', len(beamData[b][s])
            assert numChans == len(beamData[b][s])


    for s in range(numSamples):
        pulsarData.append([]) # start a new time sample
        for b in range(numBeams):
            # create this samples data
            pulsarData[s].append(beamData[b][s])

    return (pulsarData, numSamples, numChans)
    
def getSimplePulsarData(numSamples, numBeams, numChans):
    """
    create some pulsar data that has unique values
    in an arbitrary format
    """
    pulsarData = []
    for s in range(numSamples):
        pulsarData.append([]) # start a new time sample
        for b in range(numBeams):
            # create this samples data
            pulsarData[s].append([c + (b*numChans) for c in range(numChans)])
    return pulsarData        

simpleData = 0
numBeams = 7
numChans = 500 #97
#numSamples = 2
#pulsarData = getSimplePulsarData(numSamples, numBeams, numChans)
pulsarData, numSamples, numChans = getFakePulsarDataFromFiles(numBeams, numChans)
for s in range(numSamples):
    for b in range(numBeams):
        print "sample %d, beam %d length: %d" % (s, b, len(pulsarData[s][b]))
print numSamples, numChans
#import sys
#sys.exit(0)

# now convert this arbitrary format to our expect BF Pulsar FITS format

banks = ['A','B','C','D','E','F','G','H','I','J']

# init a dict. to hold the data in memory before going to the FITS file
fitsData = []
# top level organized by time sample
for s in range(numSamples):
    # then organized by beam
    fitsData.append({})
    for i in range(numBeams):
        fitsData[s][str(i)] = {}
        # and then Bank - that is, by frequency channel
        for b in banks:
            fitsData[s][str(i)][b] = []

# recall that freq. channels are in chunks of 5 contigous channels
chunkSize = 5 # channels
chunkStep = 50 
numChunks = 10
bankStepSize = 5

# get the data organized properly
print "Sample, Beam #, Bank Index, Chunk Index, start, end, values: " 
for s in range(numSamples):
    for bm in range(numBeams):
        print "****** Beam: ", bm
        for bi, bk in enumerate(banks):
            for ci in range(numChunks):
                start = (bi * bankStepSize) + (ci * chunkStep)
                end = start + chunkSize 
                print s, bm, bi, ci, start, end, pulsarData[s][bm][start:end]
                
                fitsData[s][str(bm)][bk].extend(pulsarData[s][bm][start:end])
        #for bk in banks:        
        #    print "len of beam data: ", bm, bk, len(fitsData[str(bm)][bk])            

# now use 'fitsData' to get these into FITS files properly         
fitsFiles = {}
for bk in banks:
    fitsFiles[bk] = []
    for s in range(numSamples):
        fitsFiles[bk].append([])
        for ci in range(numChunks*chunkSize):
            for bm in range(numBeams):
                fitsFiles[bk][s].append(fitsData[s][str(bm)][bk][ci])

# Tests
# make sure you have all the data, nothing but the data
# just one sample for now
s = 0
allData = []
for bk in banks:
    #for d in fitsFiles[bk][s]:
    allData.extend(fitsFiles[bk][s])
        #if d not in allData:
        #    print "appending data for bk, s: ", bk, s, d
        #    allData.append(d)

print len(allData), numBeams * numChans
assert len(allData) == numBeams * numChans

if not simpleData:
    import sys
    sys.exit(0)

assert sorted(allData) == range(numBeams * numChans)

# now do some basic tests
# TBF: use numpy?
baseData = [i*numChans for i in range(numBeams)]
for bi, bk in enumerate(banks):
    assert len(fitsFiles[bk][s]) == numChunks*chunkSize*numBeams
    for ci in range(numChunks):
        for cii in range(chunkSize):
            i = (ci * chunkStep) + cii
            j = (ci * chunkSize) + cii
            expData = [(b+i) + (bi*chunkSize) for b in baseData]
            assert fitsFiles[bk][s][j*numBeams:(j+1)*numBeams] == expData

            
print "All Tests Passed."            
        
    
    


