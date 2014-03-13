#include "SwitchingStateMachine.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define CAL_BIT (1)
#define SR_BIT_MASK (0x1)
#define CAL_BIT_MASK (0x2)
#define SIG_REF_CAL_MASK (SR_BIT_MASK|CAL_BIT_MASK)
#define ACCUMID_XOR_MASK 0x3

int32_t sigref_cal_to_accumid(int32_t sr, int32_t cal)
{
    return (sr | (cal << 1)) ^ ACCUMID_XOR_MASK;
}

void accumid_to_sigref_cal(int32_t accumid, int32_t *sr, int32_t *cal)
{
    int32_t srcal = accumid ^ ACCUMID_XOR_MASK;
    *sr = srcal & SR_BIT_MASK ? 1 : 0;
    *cal = srcal & CAL_BIT_MASK ? 1: 0;
}


SwitchingStateMachine *
create_switching_state_machine(int32_t nphases, int32_t *sref, int32_t *cal, 
                               int32_t num_swperiods_per_exp, int64_t counts_per_exp)
{
    int32_t i;
    SwitchingStateMachine *p;
    
    if (nphases <1)
    {
        printf("SwitchingStateMachine: nphases must be between two and MAX_PHASES\n");
        return 0;
    }
    p = (SwitchingStateMachine *)malloc(sizeof(SwitchingStateMachine));
    if (sref)
        memcpy(p->sig_ref_table, sref, sizeof(int)*nphases);
    else
        memset(p->sig_ref_table, 0, sizeof(int)*nphases);
    if (cal)
        memcpy(p->cal_table, cal, sizeof(int)*nphases);
    else
        memset(p->cal_table, 0, sizeof(int)*nphases);
    
    p->cur_sw_cycle_number=1;
    p->switch_periods_per_exposure = num_swperiods_per_exp;
    p->nphases = nphases;
    p->cur_phase_idx=0;
    p->prior_phase_idx=0;
    p->counts_per_exposure = counts_per_exp;
    p->end_exposure_count = counts_per_exp;
    p->last_sw_transition_count = -1;
    p->last_exposure_count = -1;
    p->approximate_counts_per_cycle = 
                       p->counts_per_exposure/p->switch_periods_per_exposure;
    
    for (i=0; i<nphases; ++i)
    {
        p->accumid_table[i] = sigref_cal_to_accumid(p->sig_ref_table[i],
                                                    p->cal_table[i]);
    }
    p->cur_accumid = p->accumid_table[0];
    return p;    
}

void destroy_switching_state_machine(SwitchingStateMachine *p)
{
    if (p==0)
    {
        return;
    }
    free(p);
}

int32_t exposure_by_counts(SwitchingStateMachine *p, int64_t count)
{
    int32_t exposure_complete = 0;
    if (count >= p->end_exposure_count)
    {
        exposure_complete = 1;
        do
        {
            p->end_exposure_count += p->counts_per_exposure;
        } while (count > p->end_exposure_count);
    }
    return exposure_complete;
}

int32_t exposure_by_phases_v1(SwitchingStateMachine *p, int32_t accumid, int64_t count)
{
    if (p->cur_accumid == (accumid & SIG_REF_CAL_MASK))
    {
        return 0;
    }
    // OK accumid's are different, we have a new phase
    p->cur_accumid = accumid & SIG_REF_CAL_MASK;
    p->cur_phase_idx++;
    // Is this the last phase of the sw cycle?
    if (p->cur_phase_idx == p->nphases)
    {
        // increment the sw cycle count
        p->cur_sw_cycle_number++;
        p->cur_phase_idx = 0;
        // Have we seen enough sw cycles?
        // printf("cursw=%d, need=%d\n", p->cur_sw_cycle_number , p->switch_periods_per_exposure);
        if (p->cur_sw_cycle_number > p->switch_periods_per_exposure)
        {
            p->cur_sw_cycle_number=1;
            // exposure is complete
            return 1;
        }
    }
    return 0;
}

int32_t exposure_by_phases_v2(SwitchingStateMachine *p, int32_t in_accumid, int64_t count)
{
    int32_t i;
    int32_t in_phase_idx=-1;
    // mask out blanking bits
    int32_t accumid = in_accumid & SIG_REF_CAL_MASK;
    // being used???
    p->cur_accumid = accumid;
    p->cur_count = count; // record the clock/count
    
    // search the accumid table to determine which phase we are in
    for (i=0; i<p->nphases; ++i)
    {
        if (p->accumid_table[i] == accumid)
        {
            in_phase_idx = i;
            break;            
        }
    }
    if (i == p->nphases)
    {
        printf("Unknown accumid state: %d \n", i);
        return 0; // PUNT ???
    }
    // check to see if we are already in that phase (naive)
    if (p->cur_phase_idx == in_phase_idx)
    {
        return 0;
    }
    printf("new phase %d count=%ld\n", in_phase_idx, count);
    if ((p->cur_phase_idx+1)%p->nphases != in_phase_idx)
    {
        printf("Looks like we missed a phase: in=%d cur=%d\n", 
                in_phase_idx, (p->cur_phase_idx+1)%p->nphases);
    }
    // update current phase index
    p->cur_phase_idx = in_phase_idx;

    // Is this the last phase of the sw cycle?
    if (p->cur_phase_idx == 0)
    {
        // increment the sw cycle count
        p->cur_sw_cycle_number++;
        p->last_sw_transition_count = count;
        // p->cur_phase_idx = 0;
        // Have we seen enough sw cycles?
        // printf("cursw=%d, need=%d\n", p->cur_sw_cycle_number , p->switch_periods_per_exposure);
        if (p->cur_sw_cycle_number > p->switch_periods_per_exposure)
        {
            p->cur_sw_cycle_number=1;
            p->last_exposure_count = count;
            // exposure is complete
            return 1;
        }
    }
    return 0;
}

int32_t new_input_state(SwitchingStateMachine *p, int accumid, int64_t count)
{
    p->cur_count = count;
    // If there is no switching, fall back to a count/clock-based method
    if (p->nphases < 2)
    {
        return exposure_by_counts(p, count);
    }
    else
    {
        return exposure_by_phases_v2(p, accumid, count);
    }
}
    
