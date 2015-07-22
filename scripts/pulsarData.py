# create some pulsar data that has unique values
# in an arbitrary format
pulsarData = []
numBeams = 7
numChans = 500
for b in range(numBeams):
    pulsarData.append([c + (b*numChans) for c in range(numChans)])

# now convert this arbitrary format to our expect BF Pulsar FITS format

banks = ['A','B','C','D','E','F','G','H','I','J']

# init a dict. to hold the data in memory before going to the FITS file
fitsData = {}
# organized by beam
for i in range(numBeams):
    fitsData[str(i)] = {}
    # and then Bank - that is, by frequency channel
    for b in banks:
        fitsData[str(i)][b] = []

# recall that freq. channels are in chunks of 5 contigous channels
chunkSize = 5 # channels
chunkStep = 50 
numChunks = 10
bankStepSize = 5

# get the data organized properly
print "Beam #, Bank Index, Chunk Index, start, end, values: " 
for bm in range(numBeams):
    print "****** Beam: ", bm
    for bi, bk in enumerate(banks):
        for ci in range(numChunks):
            start = (bi * bankStepSize) + (ci * chunkStep)
            end = start + chunkSize 
            print bm, bi, ci, start, end, pulsarData[bm][start:end]
            fitsData[str(bm)][bk].extend(pulsarData[bm][start:end])
    #for bk in banks:        
    #    print "len of beam data: ", bm, bk, len(fitsData[str(bm)][bk])            

# now use 'fitsData' to get these into FITS files properly         
fitsFiles = {}
for bk in banks:
    fitsFiles[bk] = []
    for ci in range(numChunks*chunkSize):
        for bm in range(numBeams):
            fitsFiles[bk].append(fitsData[str(bm)][bk][ci])

# now do some basic tests
# TBF: use numpy?
baseData = [i*numChans for i in range(numBeams)]
for bi, bk in enumerate(banks):
    assert len(fitsFiles[bk]) == numChunks*chunkSize*numBeams
    for ci in range(numChunks):
        for cii in range(chunkSize):
            i = (ci * chunkStep) + cii
            j = (ci * chunkSize) + cii
            expData = [(b+i) + (bi*chunkSize) for b in baseData]
            assert fitsFiles[bk][j*numBeams:(j+1)*numBeams] == expData

# finally, make sure you have all the data, nothing but the data
allData = []
for bk in banks:
    for d in fitsFiles[bk]:
        if d not in allData:
            allData.append(d)
assert len(allData) == numBeams * numChans
assert sorted(allData) == range(numBeams * numChans)
            
print "All Tests Passed."            
        
    
    


