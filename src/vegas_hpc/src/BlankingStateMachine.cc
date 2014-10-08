
#include "BlankingStateMachine.h"

#include <stdio.h>

#define FFT_BLANKING_CYCLES 0


BlankingStateMachine::BlankingStateMachine() :
    cur_state(Blanking),
    prev_state(Blanking),
    blanking_counter(0),
    prev_sw_status(0x8000)
{

}

static const char *state_names[] = {"NotBlanking", "Blanking", "WaitBlank"};

BlankingState
BlankingStateMachine::get_state()
{
    return cur_state;
}

const char *
BlankingStateMachine::get_state_name()
{
    return state_names[cur_state];
}

int 
BlankingStateMachine::blank_current_fft()
{
    return (cur_state == Blanking || cur_state == WaitBlank);
}

int
BlankingStateMachine::sw_status_changed(int sw_status)
{
    int new_state = (sw_status & 0x3) != prev_sw_status ? 1 : 0;
    if (prev_sw_status == 0x8000)
        new_state = 0;
    prev_sw_status = sw_status & 0x3;
    return new_state;
}

void
BlankingStateMachine::new_input(int blank_status)
{
    BlankingState next_state = cur_state;
    prev_state = cur_state;
    int blanked_at_start    = blank_status & 0x2;
    int is_blanked_anywhere = blank_status & 0x1;
    int sw_state_changed    = blank_status & 0x4;
    
    switch (cur_state)
    {
        case NotBlanking:
            // We currently are not blanking, so now we check the input to determine
            // If we need to change state:
            if (is_blanked_anywhere || sw_state_changed)
            {
                reset_blanking_cycle();
                if (blanked_at_start)
                    next_state = Blanking;
                else
                    next_state = WaitBlank;
                    // next_state = Blanking;
            }        
        break;
        
        case WaitBlank:
            // Here we are waiting until we see blanking in the first time sample of the current input
            // Once detected, we can begin counting down blanked fft's
            if (blanked_at_start)
            {
                next_state = Blanking;
                blanking_counter--;
            }
        break;
        
        case Blanking:

            if (sw_state_changed)
            {
                reset_blanking_cycle();
            }
            if (!is_blanked_anywhere && blanking_counter < 1)
            {
                next_state = NotBlanking;
                blanking_counter = 0;
            }
            blanking_counter--;            
        break;
    }
    cur_state=next_state; 
    /* DEBUG
    printf("BS: %s %s %s\n",
           blank_current_fft() ? "BlankFFT" : "NoblankFFT",
           needs_flush()       ? "Flush"   : "NoFsh",
           get_state_name());
    */

} 


// Returns non-zero when a rising edge on blanking or state change is noted
int BlankingStateMachine::needs_flush()
{
    if (prev_state == NotBlanking && (cur_state == Blanking || cur_state == WaitBlank))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void
BlankingStateMachine::reset()
{
    blanking_counter = 0;
    cur_state = Blanking;
    prev_state = Blanking;
}

void 
BlankingStateMachine::reset_blanking_cycle()
{
    blanking_counter = FFT_BLANKING_CYCLES;
}


