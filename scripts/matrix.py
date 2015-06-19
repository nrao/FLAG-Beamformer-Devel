# N = 8
# M = 8

# n = N/2
# m = M/2

# 



# for dd in d:
#     print dd
#             



# def get_sub_matrix(row, col):
#   for xi in range(1, 3):
#       for yi in range(1, 3):
#           if xi != yi:
#               print (xi + row*2, yi + col*2),
#       print



# print

# redundant_blocks = []
# next_red = 1
# inc = 8
# for i in range(820):
#   if i == next_red:
#       next_red += inc
#       inc += 4
#       redundant_blocks.append(i)
#       print i


# get_sub_matrix(1, 0)



n = 4
m = n

# matrix = []

# # for ni in range(1, n + 1):
# #     for mi in range(1, ni + 1):
# #         print (ni,mi)
# d = []

def get_input_coeff(ni, mi):
    d = []
    d.append((2*ni,2*mi))
    if mi != ni:
        d.append((2*ni,2*mi+1))
    d.append((2*ni+1,2*mi))
    d.append((2*ni+1,2*mi+1))
    return d

# for dd in range(d)

def get_old_index(n, m):
    old_index = 0
    for i in range(n , 0, -1):
        old_index += i

    old_index += m
    print "old_index for (%d, %d): %d" % (n, m, old_index)
    return old_index

def get_old_indices(n, m):
    indices = []
    for dd in get_input_coeff(n, m):
        # print dd
        indices.append(get_old_index(dd[0], dd[1]))

    return indices

bf_cov = []
for i in range(m):
    for j in range(i + 1):
        bf_cov.extend(get_old_indices(i, j))

for block in bf_cov:
    print block, " ", 

# get_old_indices(2,0)