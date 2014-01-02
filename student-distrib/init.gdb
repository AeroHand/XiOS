# vim: ft=gdb
# display the current instruction
#display /i $pc
# display the first 12 words on the stack
#display /12w $esp
# display the location of the current code
#display $eip
set $tss = (tss_t*) tss
#break adjust_scrollback_page
