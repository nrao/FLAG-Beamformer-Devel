function c_to_ri_xblock(n_bits, bin_pt)



%% inports
xlsub4_c = xInport('c');

%% outports
xlsub4_re = xOutport('re');
xlsub4_im = xOutport('im');

%% diagram

% block: half_band_xblock/Subsystem/parallel_fir/c_to_ri1/force_im
xlsub4_slice_im_out1 = xSignal;
xlsub4_force_im = xBlock(struct('source', 'Reinterpret', 'name', 'force_im'), ...
                         struct('force_arith_type', 'on', ...
                                'arith_type', 'Signed  (2''s comp)', ...
                                'force_bin_pt', 'on', ...
                                'bin_pt', bin_pt), ...
                         {xlsub4_slice_im_out1}, ...
                         {xlsub4_im});

% block: half_band_xblock/Subsystem/parallel_fir/c_to_ri1/force_re
xlsub4_slice_re_out1 = xSignal;
xlsub4_force_re = xBlock(struct('source', 'Reinterpret', 'name', 'force_re'), ...
                         struct('force_arith_type', 'on', ...
                                'arith_type', 'Signed  (2''s comp)', ...
                                'force_bin_pt', 'on', ...
                                'bin_pt', bin_pt), ...
                         {xlsub4_slice_re_out1}, ...
                         {xlsub4_re});

% block: half_band_xblock/Subsystem/parallel_fir/c_to_ri1/slice_im
xlsub4_slice_im = xBlock(struct('source', 'Slice', 'name', 'slice_im'), ...
                         struct('nbits', n_bits, ...
                                'mode', 'Lower Bit Location + Width'), ...
                         {xlsub4_c}, ...
                         {xlsub4_slice_im_out1});

% block: half_band_xblock/Subsystem/parallel_fir/c_to_ri1/slice_re
xlsub4_slice_re = xBlock(struct('source', 'Slice', 'name', 'slice_re'), ...
                         struct('nbits', n_bits), ...
                         {xlsub4_c}, ...
                         {xlsub4_slice_re_out1});


end

