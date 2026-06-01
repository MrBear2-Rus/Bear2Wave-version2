# Bear2Wave demo script (requires BEAR2WAVE_WITH_TCL build)
# Adjust path to your checkout:

# repo root is three levels up from TEST1/examples/scripts/
set trace [file normalize [file join [file dirname [info script]] .. .. .. test2.vcd]]

bear2wave_echo "loading $trace"
bear2wave_load $trace

gtkwave::addSignalsFromList {TOP.clk TOP.rst_n}

bear2wave_zoom full
bear2wave_set_time 40
bear2wave_page right
bear2wave_echo "demo done"
