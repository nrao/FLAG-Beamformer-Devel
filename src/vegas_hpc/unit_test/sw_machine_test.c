#include <stdio.h>
#include "SwitchingStateMachine.h"

int test_exposures()
{
    int sr[4] =  {1,1,0,0};
    int cal[4] = {0,1,0,1};
    int nphases = 4;
    int ncycles = 1;
    int i, j, z;
    int exp_rdy;
    int counts = 0;
    
    SwitchingStateMachine *p = create_switching_state_machine(nphases, sr, cal, ncycles, 0);
    if (!p)
    {
        printf("create failed\n");
        return 0;
    }
    for (z=0; z<10; ++z)
    {
        for (i=0; i<nphases; ++i)
        {
            for (j=0; j<10; ++j)
            {
                // printf("i=%d,j=%d,z=%d\n", i,j,z);
                exp_rdy = new_input_state(p, p->accumid_table[i], counts);
                if (exp_rdy)
                    printf("exp_ready, count=%ld i=%d, j=%d, z=%d\n", counts,i,j,z);
                counts += 1000;
            }
        }
    }
    if(new_input_state(p, p->accumid_table[0], counts))
        printf("last exp_ready,\n");

    return 0;
}

int test_exposures_with_skipped_phases()
{
    int sr[4] =  {1,1,0,0};
    int cal[4] = {0,1,0,1};
    int nphases = 4;
    int ncycles = 2;
    int i, j, z;
    int exp_rdy;
    int counts = 0;
    
    SwitchingStateMachine *p = create_switching_state_machine(nphases, sr, cal, ncycles, 0);
    if (!p)
    {
        printf("create failed\n");
        return 0;
    }
    for (z=0; z<10; ++z)
    {
        for (i=0; i<nphases; ++i)
        {
            for (j=0; j<1; ++j)
            {
                // printf("i=%d,j=%d,z=%d\n", i,j,z);
                if (i==2 && z==2)
                {
                    counts += 1000;
                    continue;
                }
                else
                {
                    exp_rdy = new_input_state(p, p->accumid_table[i], counts);
                }
                if (exp_rdy)
                    printf("exp_ready, count=%ld i=%d, j=%d, z=%d\n", counts,i,j,z);
                counts += 1000;
            }
        }
    }
    if(new_input_state(p, p->accumid_table[0], counts))
        printf("last exp_ready,\n");

    return 0;
}

extern int sigref_cal_to_accumid(int sr, int cal);
extern void accumid_to_sigref_cal(int accumid, int *sr, int *cal);

void test_conversions()
{
    int sigref[4] = {1,1,0,0};
    int cal[4]    = {0,1,0,1};
    int accumid[4] = {2,0,3,1};
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
    test_exposures();
    printf("test_exposures_by_counts\n");
    test_exposures_by_counts();
    printf("test_exposures_with_skipped_phases\n");
    test_exposures_with_skipped_phases();
    return 0;
}
