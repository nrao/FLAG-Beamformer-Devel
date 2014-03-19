#include <stdio.h>
#include "SwitchingStateMachine.h"

int test_exposure_by_phase()
{
    int sr[4] =  {1,1,0,0};
    int cal[4] = {0,1,0,1};
    int nphases = 4;
    int ncycles = 1;
    int i, j, z;
    int exp_rdy;
    int counts = 1;
    int64_t approx_counts_per_exp = ncycles * 10000 * nphases;
    
    printf("Test conditions: %d phases, %d cycles, expect exposures every %ld counts\n",
           nphases, ncycles, approx_counts_per_exp);
    SwitchingStateMachine *p = create_switching_state_machine(nphases, sr, cal, ncycles, approx_counts_per_exp);
    if (!p)
    {
        printf("create failed\n");
        return 0;
    }
    for (z=0; z<10; ++z)
    {
        for (i=0; i<nphases; ++i)
        {
            for (j=0; j<10; ++j, counts += 1000)
            {
                // printf("i=%d,j=%d,z=%d\n", i,j,z);
                exp_rdy = new_input_state(p, p->accumid_table[i], counts);
                if (exp_rdy)
                    printf("exp_ready, count=%ld i=%d, j=%d, z=%d\n", counts,i,j,z);
            }
        }
    }
    if(new_input_state(p, p->accumid_table[0], counts))
        printf("last exp_ready counts=%ld,\n", counts);

    return 0;
}

int test_exposures_with_skipped_phases(int nphases, int ncycles, 
                                       int64_t bias, int clockfactor,
                                       int64_t start_dropout,
                                       double dropped_phases)
{
    int sr[4] =  {1,1,0,0};
    int cal[4] = {0,1,0,1};
    int nreps = 10;
    int i, j, z;
    int exp_rdy;
    int64_t counts = bias;
    int64_t counts_per_phase = nreps*clockfactor;
    int64_t counts_per_cycle = counts_per_phase*nphases;
    int64_t counts_per_exp   = counts_per_cycle*ncycles;
    int64_t end_dropout = start_dropout + (int64_t)(counts_per_phase * dropped_phases);                          + counts_per_phase/2;
    
    
    printf("Test conditions:nphases=%d, ncycles=%d, sdrop=%ld dphases=%f expcnt=%ld\n",
           nphases, ncycles, start_dropout, dropped_phases, counts_per_exp);
    
    SwitchingStateMachine *p = 
        create_switching_state_machine(nphases, sr, cal, ncycles, counts_per_exp);
    if (!p)
    {
        printf("create failed\n");
        return 0;
    }
    for (z=0; z<20; ++z)
    {
        for (i=0; i<nphases; ++i)
        {
            for (j=0; j<10; ++j, counts += 1000)
            {
                // printf("i=%d,j=%d,z=%d\n", i,j,z);
                // drop 2 sw cycles
                if (counts >= start_dropout && 
                    counts <= end_dropout)
                {
                    printf("dropped %ld\n", counts-bias);
                    continue;
                }
                else
                {
                    exp_rdy = new_input_state(p, p->accumid_table[i], counts);
                }
                if (exp_rdy)
                    printf("exp_ready, count=%ld i=%d, j=%d, z=%d\n", counts-bias,i,j,z);
                
            }
        }
    }
    if(new_input_state(p, p->accumid_table[0], counts))
        printf("last exp_ready %ld,\n",counts-bias);

    return 0;
}

extern int sigref_cal_to_accumid(int sr, int cal);
extern void accumid_to_sigref_cal(int accumid, int *sr, int *cal);

void test_conversions()
{
    int sigref[4] = {1,1,0,0};
    int cal[4]    = {0,1,0,1};
    int accumid[4] = {1,0,3,2};
    int i;
    int accum;
    int sr,cl;
    
    for (i=0; i<4; ++i)
    {
        accum = sigref_cal_to_accumid(sigref[i], cal[i]);
        if (accum != accumid[i])
        {
            printf("sigref_cal_to_accumid error sr=%d cal=%d acc=%d, expected=%d\n", 
                   sigref[i], cal[i],accum,accumid[i]);
        }
        accumid_to_sigref_cal(accum, &sr, &cl);
        if (sr != sigref[i] || cl != cal[i])
        {
            printf("accumid_to_sigref_cal error sr=%d cal=%d acc=%d\n", 
                   sr, cl,accum);
        }
    }
}

int test_exposures_by_counts()
{
    int sr[4] =  {1,1,0,0};
    int cal[4] = {0,1,0,1};
    int nphases = 1;
    int ncycles = 1;
    int i, j, z;
    int exp_rdy;
    int64_t counts = 0;
    int64_t counts_per_exp = 100000;
    
    printf("Test conditions: counts per exposure = %ld\n", counts_per_exp);
    SwitchingStateMachine *p = create_switching_state_machine(nphases, 0, 0, 1, counts_per_exp);
    if (!p)
    {
        printf("create failed\n");
        return 0;
    }
    for (z=0; z<10; ++z)
    {
        for (i=0; i<10; ++i)
        {
            counts += 10000;
            exp_rdy = new_input_state(p, 0, counts);
            if (exp_rdy)
                printf("cnt_exp_ready, z=%d i=%d counts=%ld\n", z, i, counts);

        }
    }
    counts+= 10000;
    if(new_input_state(p, 0,counts))
        printf("last cnt_exp_ready,\n");

    return 0;
}


int main(int argc, char **argv)
{
    
    test_conversions();
    printf("test_exposures by phase\n");
    test_exposure_by_phase();
    printf("test_exposures_by_counts\n");
    test_exposures_by_counts();
    printf("test_exposures_with_skipped_phases\n");
    // 4 phases, 1 cycle, initcnt 15000, clock 1000, start cnt 0, end 5.5 phases later
    test_exposures_with_skipped_phases(4, 1, 10000, 1000, 0, 5.5);
    test_exposures_with_skipped_phases(4, 1, 10000, 1000, 559, 5.5);
    test_exposures_with_skipped_phases(4, 2, 10000, 950, 559, 11.5);
    test_exposures_with_skipped_phases(4, 2, 10000, 1000, 559, 11.5);
    test_exposures_with_skipped_phases(4, 2, 10000, 1100, 559, 11.5);
    test_exposures_with_skipped_phases(4, 3, 10000, 1000, 559, 15.5);
    return 0;
}
