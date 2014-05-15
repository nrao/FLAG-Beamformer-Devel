#include "SwitchingStateMachine.h"
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define SR_BIT_MASK (0x2)
#define CAL_BIT_MASK (0x1)
#define SIG_REF_CAL_MASK (SR_BIT_MASK|CAL_BIT_MASK)
#define ACCUMID_XOR_MASK 0x3

int32_t sigref_cal_to_accumid(int32_t sr, int32_t cal)
{
    return (cal | (sr << 1)) ^ ACCUMID_XOR_MASK;
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
    int64_t approximate_counts_per_cycle = counts_per_exp/(int64_t)num_swperiods_per_exp;
    
    if (nphases <1)
    {
        printf("SwitchingStateMachine: nphases must be between one and MAX_PHASES\n");
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
    
    p->cur_sw_cycle_number=0;
    p->switch_periods_per_exposure = num_swperiods_per_exp;
    p->nphases = nphases;
    p->cur_phase_idx=0;
    p->prior_phase_idx=0;
    p->counts_per_exposure = counts_per_exp;
    p->end_exposure_count = counts_per_exp;
    p->last_sw_transition_count = -1;
    p->last_exposure_count = -1;
    p->last_count = 0;
    
    approximate_counts_per_cycle = counts_per_exp/num_swperiods_per_exp;
    p->approximate_counts_per_cycle = approximate_counts_per_cycle;
    p->lower_counts_per_cycle = (int32_t)(approximate_counts_per_cycle * 0.8 + 0.5);
    p->upper_counts_per_cycle = (int32_t)(approximate_counts_per_cycle * 1.2 + 0.5);
    
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
        if (p->cur_sw_cycle_number >= p->switch_periods_per_exposure)
        {
            p->cur_sw_cycle_number=0;
            // exposure is complete
            return 1;
        }
    }
    return 0;
}

/* Smarter routine below

    Missed phase analsis:
        if cur_phase is equal to new_phase, then verify time didn't jump by an entire
        switching cycle. If it did, calculate the number of cycles + add to cycle count.
        
        if new_phase is 'behind' e.g not greater than the cur_phase, then we must have
        crossed a switching boundry. Increament cycle count and see if more than
        one cycle has been lost.
        
        if new_phase is 'ahead', and clock diff is < 1 cycle, all is good.
*/
int32_t exposure_by_phases_v2(SwitchingStateMachine *p, int32_t in_accumid, int64_t count)
{
    int32_t i;
    int32_t in_phase_idx=-1;
    int64_t ncount_diff;
    int32_t nphases = p->nphases;
    
    // mask out blanking bits
    int32_t accumid = in_accumid & SIG_REF_CAL_MASK;
    // being used???
    p->cur_accumid = accumid;
    p->cur_count = count; // record the clock/count
    
    // search the accumid table to determine which phase we are in
    for (i=0; i<nphases; ++i)
    {
        if (p->accumid_table[i] == accumid)
        {
            in_phase_idx = i;
            break;            
        }
    }
    // How long has it been since the last input?
    ncount_diff = count - p->last_count;
    p->last_count = count;
    
    if (i >= nphases)
    {
        printf("Unknown accumid state: %d \n", i);
        return 0; // PUNT ???
    }
    if (ncount_diff == 0)
    {
        printf("ncount_diff is zero - counter stuck???\n");
        p->cur_phase_idx = in_phase_idx;
        return 0; // PUNT
    }

    int64_t counts_per_phase = p->approximate_counts_per_cycle/p->nphases;    
    int64_t missed_phases = ncount_diff/counts_per_phase; 
    int32_t correction_made = 0;
    int32_t exposures_complete = 0;

    if (missed_phases < 0)
    {
        printf("very odd missed phases < 0 case - this should never happen %ld\n", missed_phases);
    }
    // has more than one phase time elapsed since the last input?
    // If so, sequence through the phases as we normally would, and
    // count switching cycle last->first phase transitions, beginning with
    // the last phase state seen.
    while (missed_phases>0LL)
    {
        p->cur_phase_idx = (p->cur_phase_idx+1)%nphases;
        // has a sw_cycle boundary been crossed?
        if (p->cur_phase_idx == 0) 
        {
            p->cur_sw_cycle_number++;
        }
        if (p->cur_sw_cycle_number >= p->switch_periods_per_exposure)
        {
            exposures_complete++;
            // Keep the cycle numbers modulo n_cycles needed per exposure
            p->cur_sw_cycle_number=p->cur_sw_cycle_number%p->switch_periods_per_exposure;
        }
        missed_phases--;
        correction_made++;
    }
    if (correction_made && p->cur_phase_idx != in_phase_idx)
    {
        // This should only be an issue if we under corrected across a cycle boundry
        // In the over-correction case, cur_phase_idx is 0 (first phase) and the
        // in_phase_idx is the last phase.
        // In the under-correction case, in_phase_idx is 0, and the cur_phase_idx is
        // the last phase.
        // If we over-corrected we need to decrement the cycle count, and possibly
        // undo exposure_complete if cur_sw_cycle is 0.
        // If we under-corrected, we need to increment the cycle count and check
        // to see if we need to increment exposure_complete. Tedious.
        int32_t end_phase = nphases-1;        
        int32_t over_corrected = 0;
        int32_t under_corrected = 0;
        if (p->cur_phase_idx == 0 && in_phase_idx == end_phase)
        {
            over_corrected = 1;
            p->cur_sw_cycle_number--; 
            
        }
        else if (p->cur_phase_idx == end_phase && in_phase_idx == 0)
        {
            under_corrected = 1;
            p->cur_sw_cycle_number++;
        }
        if (over_corrected && exposures_complete && p->cur_sw_cycle_number < 0)
        {
            // We over-corrected across an exposure boundary
            p->cur_sw_cycle_number =0;
            exposures_complete--;
        }
        if (under_corrected && p->cur_sw_cycle_number >= p->switch_periods_per_exposure)
        {
            // We under-corrected across an exposure boundary
            exposures_complete++;
            p->cur_sw_cycle_number=p->cur_sw_cycle_number%p->switch_periods_per_exposure;
        } 
    }

    // Did we finish the the last phase of the sw cycle (and are starting the next)?
    if (!correction_made && in_phase_idx == 0 && (p->cur_phase_idx != in_phase_idx))
    {
        // increment the sw cycle count
        p->cur_sw_cycle_number++;
    }
    
    // update current phase index
    p->cur_phase_idx = in_phase_idx;

    p->last_sw_transition_count = count;
    // p->cur_phase_idx = 0;
    // Have we seen enough sw cycles?
    if (p->cur_sw_cycle_number >= p->switch_periods_per_exposure || exposures_complete>0)
    {
        p->cur_sw_cycle_number=p->cur_sw_cycle_number%p->switch_periods_per_exposure;
        p->last_exposure_count = count;
        // exposure is complete
        return 1;
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
    
