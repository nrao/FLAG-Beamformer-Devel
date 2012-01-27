function [head_count_rst,head_count_en,num_payload_rst,num_payload_en,words_sent_rst,words_sent_en,tge_valid,heap_elements,select_data,st] = spead_fsm_v2(head_count,num_payload_sent,total_header_len,min_header_len,payload_done,all_payloads_done,data_valid_in,rst)

persistent state, state=xl_state(0,{xlUnsigned,3,0});

state_reset         = 0;
state_long_header   = 1;
state_short_header  = 2;
state_ready_to_send = 3;
state_payload       = 4;
state_done          = 5;

switch double(state)
    case state_reset
        head_count_rst  = true;
        head_count_en   = false;
        num_payload_rst = true;
        num_payload_en  = false;
        words_sent_rst  = true;
        words_sent_en   = false;
        tge_valid       = false;
        select_data     = 0;
        heap_elements   = 0;
        
        state           = state_long_header;
    
    case state_long_header
        head_count_rst  = false;
        head_count_en   = true;
        num_payload_rst = false;
        num_payload_en  = false;
        words_sent_rst  = false;
        words_sent_en   = false;
        tge_valid       = true;
        select_data     = 0;
        heap_elements   = total_header_len-1;
        
        if head_count == total_header_len-1
            state = state_ready_to_send;
        end
        
    case state_short_header
        head_count_rst  = false;
        head_count_en   = true;
        num_payload_rst = false;
        num_payload_en  = false;
        words_sent_rst  = false;
        words_sent_en   = false;
        tge_valid       = true;
        select_data     = 0;
        heap_elements   = min_header_len-1;
        
        if head_count == min_header_len-1
            state = state_ready_to_send;
        end
        
    case state_ready_to_send
        head_count_rst  = false;
        head_count_en   = false;
        num_payload_rst = false;
        num_payload_en  = false;
        words_sent_rst  = false;
        words_sent_en   = false;
        tge_valid       = false;
        select_data     = 0;
        heap_elements   = 0;
        
        state           = state_payload;
    
    case state_payload
        head_count_rst  = false;
        head_count_en   = false;
        num_payload_rst = false;
        num_payload_en  = false;
        words_sent_rst  = false;
        tge_valid       = data_valid_in;
        select_data     = 1;
        heap_elements   = 0;
        
        if data_valid_in == true
            words_sent_en = true;
        else
            words_sent_en = false;
        end
        if payload_done==true
            num_payload_en = true;
            state          = state_done;
        end
        
    case state_done
        head_count_rst  = true;
        head_count_en   = false;
        num_payload_rst = false;
        num_payload_en  = false;
        words_sent_rst  = true;
        words_sent_en   = false;
        tge_valid       = false;
        select_data     = 0;
        heap_elements   = 0;

        if all_payloads_done==true
           num_payload_rst = true;
           state           = state_long_header;
        else
           state           = state_short_header;
        end
        
    otherwise
        head_count_rst  = false;
        head_count_en   = false;
        num_payload_rst = false;
        num_payload_en  = false;
        words_sent_en   = false;
        words_sent_rst  = false;
        tge_valid       = false;
        select_data     = 0;
        heap_elements   = 0;

if (rst == true)
    state = state_reset;
end
end
st = state;