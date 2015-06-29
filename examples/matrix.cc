//# Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//#
//# This program is distributed in the hope that it will be useful, but
//# WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//# General Public License for more details.
//#
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning GBT software should be addressed as follows:
//# GBT Operations
//# National Radio Astronomy Observatory
//# P. O. Box 2
//# Green Bank, WV 24944-0002 USA

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <utility>
#include <vector>

// This duplicates the functionality of scripts/matrix.py
// Compile with:
//  $ g++ -o matrix matrix.cc -std=c++0x

std::vector< std::pair<float, float> > get_coords_in_block(int n, int m)
{
    std::vector< std::pair<float, float> > els;

    els.push_back(std::make_pair(2*n,2*m));
    if (n != m)
        els.push_back(std::make_pair(2*n, 2*m+1));
    els.push_back(std::make_pair(2*n+1, 2*m));
    els.push_back(std::make_pair(2*n+1, 2*m+1));

    return els;
}

int get_old_index(int n, int m)
{
    int old_index = 0;

    for (int i = n; i > 0; i--)
        old_index += i;
    old_index += m;

    return old_index;
}

std::vector<float> get_old_indices(int n, int m)
{
    std::vector<float> old_indices;

    std::vector< std::pair<float, float> > block_indices = get_coords_in_block(n, m);
    for (auto it = block_indices.begin(); it != block_indices.end(); it++)
        old_indices.push_back(get_old_index((*it).first, (*it).second));

    return old_indices;
}

int main()
{
    const int N = 4;
    std::vector<float> bf_cov;

    // int i;
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < i + 1; j++)
        {
            std::vector<float> new_indices = get_old_indices(i, j);
            bf_cov.insert(bf_cov.end(), new_indices.begin(), new_indices.end());
        }
    }

    for (auto it = bf_cov.begin(); it != bf_cov.end(); it++)
    {
        std::cout << *it << ", ";
    }

    std::cout << std::endl;

    return 0;

    // std::vector<float> gpu_matrix;
    // std::vector<float> fits_matrix;
    // // for (auto it = gpu_matrix.begin(); it != gpu_matrix.end(); it++)
    // // {

    // // }

    // int i, j, k;
    // for (i = 0; i < 16; i++)
    //     gpu_matrix[i] = i;

    // for (i = 0; i < 1; i++)
    // {
    //     for (j = 0; j < 4; j++)
    //     {
    //         for (k = 0; k < j+1; k++)
    //         {
    //             fits_matrix[]
    //             if (j != k)
    //                 printf("(%d, %d)\n", j, k);
    //         }
    //     }
    // }
}