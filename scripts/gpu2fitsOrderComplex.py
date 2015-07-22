# Here we try to map the contents spat out by the GPU into
# the contents of the FITS array.
# The GPU output array has both redundant data, and an odd
# ordering of elements.  
# The FITS array has no redundant data, and a sensible,
# row matrix ordering of the lower triangle of the matrix.

# Here's the algorithm:
# * First traverse a conceptual triangular matrix composed of 'blocks'
#   which are groups of four actual matrix cells, each cell representing
#   the pairing of different inputs
# * For each block:
#      * find the positions (indicies) in the GPU output array that 
#        correspond to this block
#      * find the inputs that are represented by this block.
#      * find the positions (indicies) in the FITS array that correspond
#        to each of the above inputs
#      * now that we have GPU output array indicies and FITS array indicies,
#        we simply assign the GPU value at the given index to the appropriate 
#        position in the FITS array.


def getBlockInputs(blockRow, blockCol):
    """
    For a given position in the 'block index', what are the inputs
    represented by this block, and are they all needed?  Or is
    one of them redundant?
    """
    n = blockRow
    m = blockCol
    d = []
    d.append((2*n,2*m,True))
    # Redundant elements occur only within the blocks along the
    #   diagonal of the matrix. We know that only blocks along
    #   the diagonal will have n == m
    if m != n:
        d.append((2*n,2*m+1,True))
    else:    
        d.append((2*n,2*m+1,False))
    d.append((2*n+1,2*m,True))
    d.append((2*n+1,2*m+1,True))
    return d

def getGpuArrayIndiciesFromBlock(blockRow, blockCol):
    """
    For a given postiion in the 'block matrix', what would
    be the corresponding positions in the GPU output array?
    """
    blockNum = coords2lowerTriangularOrder(blockRow, blockCol)
    base = blockNum * 4
    return range(base, base+4)
    
def getGpuArrayIndiciesFromBlockComplex(blockRow, blockCol):
    """
    For a given postiion in the 'block matrix', what would
    be the corresponding positions in the GPU output array?
    Additionally, since the Gpu data is complex, it's 
    twice as big
    """
    blockNum = coords2lowerTriangularOrder(blockRow, blockCol)
    base = blockNum * 4 * 2
    return range(base, base+(4*2))

def getInputFitsIndex(inputX, inputY):
    return coords2lowerTriangularOrder(inputX, inputY)

def getInputFitsIndexComplex(inputX, inputY):
    "Complex data means indicies need to be twice as big"
    return 2 * coords2lowerTriangularOrder(inputX, inputY)

def coords2lowerTriangularOrder(inputX, inputY):
    """
    If the lower triangle of a matrix was put in a row-order
    one dimensional array, this would map the position in the
    matrix to it's position in the 1-D array
    """
    idx = 0
    for i in range(inputX , 0, -1):
        idx += i

    idx += inputY
    return idx    

# First, test some functions
#print "coord 2,2 found at index: ", coords2lowerTriangularOrder(2,2)
print "Tests ..."
assert 5 ==  coords2lowerTriangularOrder(2,2)
inputsBlock1 = [(0, 0, True), (0, 1, False), (1, 0, True), (1, 1, True)]
inputsBlock3 = [(2, 2, True), (2, 3, False), (3, 2, True), (3, 3, True)]
assert inputsBlock1 == getBlockInputs(0,0)
assert inputsBlock3 == getBlockInputs(1,1)
print "inputs 0, 0 to fits index: ", getInputFitsIndexComplex(0, 0)
print "inputs 1, 1: ", getInputFitsIndexComplex(1, 1)
print "block 0, 0 to gpu idx: ", getGpuArrayIndiciesFromBlockComplex(0,0)
print "block 1, 1 to gpu idx: ", getGpuArrayIndiciesFromBlockComplex(1,1)

# now set up the problem
# Here's our number of inputs
M = 4
# Here we go into block space
m = M/2

# Each input pair is represented by a COMPLEX number
cmpSz = 2 # real and imaginary parts

# start off w/ Fits array being all zero
# and the size of the lower triangle of the input matrix
fitsArraySize = (M*(M+1))/2 * cmpSz
fitsArray = [0 for i in range(fitsArraySize)]

# The larger GPU array (redundant values) has values equal to it's indicies
gpuArraySize = fitsArraySize + (M/2 * cmpSz)
gpuArray = range(gpuArraySize)

print "*** Start Algo. ***"
# Here we go through the lower triangle of the *block* matrix
for bRow in range(m):
    for bCol in range(bRow + 1):
        #print "Block: ", bRow, bCol 
        # get GPU array positions and inputs represented by this block
        gpuIndx = getGpuArrayIndiciesFromBlockComplex(bRow, bCol)
        #print "gupIndx ", gpuIndx
        inputs = getBlockInputs(bRow, bCol)
        #for input, gpuIndex in zip(inputs, gpuIndx):
        for i, input in enumerate(inputs):
            #print "  i, input: ", i, input
            gpuRealIndex = gpuIndx[(i*cmpSz)]
            gpuImgIndex  = gpuIndx[(i*cmpSz)+1]
            #print "  gpu cmp indx: ", gpuRealIndex, gpuImgIndex
            x, y, needed = input
            if needed:
                # Find the FITS array positions represented in this block
                fitsArrayIndReal = getInputFitsIndexComplex(x, y)
                fitsArrayIndImg  = fitsArrayIndReal + 1
                # And now we can actually transfer values to the right position!
                #print "  inputs: ", x, y, " -> fits array idx: ", fitsArrayIndReal, fitsArrayIndImg
                fitsArray[fitsArrayIndReal] = gpuArray[gpuRealIndex]
                fitsArray[fitsArrayIndImg]  = gpuArray[gpuImgIndex]
            #else:
            #    print "  redundant: ", x, y

print "fits Array: ", fitsArray        
#fitsArrayFourInputs = [0, 2, 3, 4, 5, 8, 6, 7, 10, 11]
fitsArrayFourInputs = [0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 12, 13, 14, 15, 20, 21, 22, 23]
fitsArrayTwoInputs = fitsArrayFourInputs[:6]
#assert fitsArrayTwoInputs == fitsArray
assert fitsArrayFourInputs == fitsArray 
