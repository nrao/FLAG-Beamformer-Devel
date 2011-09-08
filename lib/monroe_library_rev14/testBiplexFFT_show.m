xx=1:length(sim2Pol_r.signals.values);



close all;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%difference-er
%diff_len = 1024*4;
diff_len = 2048;
%sync_orig = 3541-13-69-2;
sync_orig = 6000;
sync_custom = 6000 - 38+5+1-3-1+1-3;
%sync_orig = sync_custom;

xx_orig = sync_orig:(sync_orig+diff_len);
xx_cust = sync_custom:(sync_custom+diff_len);

% 
% diff_r =  sim_unscr_addsub_r.signals.values(xx_cust) - sim2_unscr_addsub_r.signals.values(xx_orig);
% diff_i =  sim_unscr_addsub_i.signals.values(xx_cust) - sim2_unscr_addsub_i.signals.values(xx_orig);



% 
% % % 
% orig_r = simout16.signals.values(xx_orig);
% orig_i = simout17.signals.values(xx_orig);
% 
% cust_r = simout28.signals.values(xx_cust)*2;
% cust_i = simout29.signals.values(xx_cust)*2;
% 
% 



% % % 
% orig_r = simout26.signals.values(xx_orig);
% orig_i = simout27.signals.values(xx_orig);
% 
% cust_r = simout14.signals.values(xx_cust)/2;
% cust_i = simout15.signals.values(xx_cust)/2;





% % % 
% orig_r = simout18.signals.values(xx_orig);
% orig_i = simout19.signals.values(xx_orig);
% 
% cust_r = simout22.signals.values(xx_cust);
% cust_i = simout23.signals.values(xx_cust);

% 
% orig_r = sim2_unscr_addsub_r.signals.values(xx_orig);
% orig_i = sim2_unscr_addsub_i.signals.values(xx_orig);
% cust_r =  simPol_r4.signals.values(xx_cust);
% cust_i =  simPol_i4.signals.values(xx_cust);



% 
orig_r = sim2Pol_r1.signals.values(xx_orig);
orig_i = sim2Pol_i1.signals.values(xx_orig);
cust_r =  simPol_r1.signals.values(xx_cust);
cust_i =  simPol_i1.signals.values(xx_cust);



diff_r =  orig_r - cust_r;
diff_i =  orig_i - cust_i;



% addr_cust = sim2_unscr_muxsel4.signals.values(xx_cust);
% addr_orig = sim2_unscr_muxsel3.signals.values(xx_orig);
% 
% muxsel_orig = sim2_unscr_muxsel.signals.values(xx_orig);
% muxsel_cust = sim_unscr_muxsel.signals.values(xx_cust);
sync_orig = simout2.signals.values(xx_orig-1024*2 );
sync_cust = simout.signals.values(xx_cust-1024*2);

% 
% figure();plot(0:diff_len, orig_r, 'b--x', ...
%               0:diff_len, orig_i, 'r');
% title('original, real/imag')
%           
%           
% figure();plot(0:diff_len,  cust_r, 'b--x', ...
%               0:diff_len,  cust_i, 'r');
% title('custom, real/imag')   
% 
% 
% 



%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

figure();plot(0:diff_len, orig_r, 'b--x', ...
              0:diff_len, sync_orig, 'g', ...
              0:diff_len, sync_cust, 'c', ...
              0:diff_len, orig_i, 'r');
title('original, real/imag')
          
          
figure();plot(0:diff_len,  cust_r, 'b--x', ...
              0:diff_len, sync_orig, 'g', ...
              0:diff_len, sync_cust, 'c', ...
              0:diff_len,  cust_i, 'r');
title('custom, real/imag')   
% 

% % % % % % % % % % % % % % % % % % 





figure();plot(0:diff_len, diff_r, 'b--x', ...
              0:diff_len, diff_i, 'r');
title('diff, real/imag')












% 
% figure();plot(xx, sim2_unscr_reorder_r.signals.values, 'b--x', ...
%               xx, sim2_unscr_reorder_i.signals.values, 'r');
% title('original, real/imag')
%           
%           
% figure();plot(xx,  sim_unscr_reorder_r.signals.values, 'b--x', ...
%               xx,  sim_unscr_reorder_i.signals.values, 'r');
% title('custom, real/imag')          
% %           
% % 
% %  
% % 
% % 
% figure();plot(xx, sim2_unscr_reorder_r.signals.values, 'b--x', ...
%               xx, sim2_unscr_reorder_i.signals.values, 'r', ...
%               xx, sim2_unscr_sync.signals.values * 1.5, 'c', ...
%               xx, sim2_unscr_muxsel.signals.values*1.1, 'g');
% title('original, real/imag')
%           
%           
% figure();plot(xx,  sim_unscr_reorder_r.signals.values, 'b--x', ...
%               xx,  sim_unscr_reorder_i.signals.values, 'r', ...
%               xx, sim_unscr_sync.signals.values * 1.5, 'c', ...
%               xx,  sim_unscr_muxsel.signals.values*1.1, 'g');
% title('custom, real/imag')          
          


% 
% 
% figure();plot(xx, sim2_unscr_reorder_r.signals.values, 'b--x', ...
%               xx, sim2_unscr_reorder_i.signals.values, 'r', ...
%               xx, sim2_unscr_muxsel.signals.values*1.1, 'g');
% title('original, real/imag')
%           
%           
% figure();plot(xx,  sim_unscr_reorder_r.signals.values, 'b--x', ...
%               xx,  sim_unscr_reorder_i.signals.values, 'r', ...
%               xx,  sim_unscr_muxsel.signals.values*1.1, 'g');
% title('custom, real/imag')          
          


%  
% 
% figure();plot(xx, sim2Pol_r.signals.values, 'b--x', ...
%               xx, sim2Pol_i.signals.values, 'r');
% title('original, real/imag')
%           
%           
% figure();plot(xx,  simPol_r.signals.values, 'b--x', ...
%               xx,  simPol_i.signals.values, 'r');
% title('custom, real/imag')          
%           
%  