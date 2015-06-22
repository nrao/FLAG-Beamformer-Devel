# Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# 
# Correspondence concerning GBT software should be addressed as follows:
#	GBT Operations
#	National Radio Astronomy Observatory
#	P. O. Box 2
#	Green Bank, WV 24944-0002 USA

#
# matrix.py
#

N = 4

# Given the coordinates of a "new" matrix block,
#   return the coordinates that are within that block as a list of tuples
def get_indices_in_block(n, m):
    d = []
    d.append((2*n,2*m))
    # Redundant elements occur only within the blocks along the
    #   diagonal of the matrix. We know that only blocks along
    #   the diagonal will have n == m
    if m != n:
        d.append((2*n,2*m+1))
    d.append((2*n+1,2*m))
    d.append((2*n+1,2*m+1))
    return d

# Given the coordinates of an element in a "new"
#   matrix block, return the index in the "old"
#   array that contains the element
def get_old_index(n, m):
    old_index = 0
    for i in range(n , 0, -1):
        old_index += i

    old_index += m
    return old_index

# Given the coordinates of a block in the "new"
#   lower triangular matrix,
#   return the indices of the "old" array that
#   map to the data in the block
def get_old_indices(n, m):
    old_indices = []
    # Get the indices of the elements contained within the
    #   "new" matrix block
    block_indices = get_indices_in_block(n, m) 
    for block_index in block_indices:
        old_indices.append(get_old_index(block_index[0], block_index[1]))

    return old_indices

# The "new" lower triangular matrix exists as a series
#   of blocks. Some contain 4 elements, others only contain
#   3 due to eliminated redundancies.
# The coordinates in these blocks can be mapped onto the data
#   existing in the "old" array of data stored in the "old" FITS
#   file.

# So, we loop through all of the blocks in the "new" matrix.
bf_cov = []
for i in range(N):
    for j in range(i + 1):
    	# For each block we get the indices of the "old" array that contain the data we are looking for.
        bf_cov.extend(get_old_indices(i, j))

# Verify output
for block in bf_cov:
    print block, 