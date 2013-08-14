library IEEE;
use IEEE.std_logic_1164.all;

entity fftwbr_core is
  port (
    ce_1: in std_logic; 
    clk_1: in std_logic; 
    in0: in std_logic_vector(17 downto 0); 
    in1: in std_logic_vector(17 downto 0); 
    in10: in std_logic_vector(17 downto 0); 
    in11: in std_logic_vector(17 downto 0); 
    in12: in std_logic_vector(17 downto 0); 
    in13: in std_logic_vector(17 downto 0); 
    in14: in std_logic_vector(17 downto 0); 
    in15: in std_logic_vector(17 downto 0); 
    in2: in std_logic_vector(17 downto 0); 
    in3: in std_logic_vector(17 downto 0); 
    in4: in std_logic_vector(17 downto 0); 
    in5: in std_logic_vector(17 downto 0); 
    in6: in std_logic_vector(17 downto 0); 
    in7: in std_logic_vector(17 downto 0); 
    in8: in std_logic_vector(17 downto 0); 
    in9: in std_logic_vector(17 downto 0); 
    shift: in std_logic_vector(31 downto 0); 
    sync_in: in std_logic; 
    of_out: out std_logic; 
    out0: out std_logic_vector(35 downto 0); 
    out1: out std_logic_vector(35 downto 0); 
    out2: out std_logic_vector(35 downto 0); 
    out3: out std_logic_vector(35 downto 0); 
    out4: out std_logic_vector(35 downto 0); 
    out5: out std_logic_vector(35 downto 0); 
    out6: out std_logic_vector(35 downto 0); 
    out7: out std_logic_vector(35 downto 0); 
    sync_out: out std_logic
  );
end fftwbr_core;

architecture structural of fftwbr_core is
begin
end structural;

