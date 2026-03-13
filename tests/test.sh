#!/bin/sh

make || { echo make failed ; exit 1; } 

tbin/test_basic.x
echo ---
tbin/test_retrieve_wait.x
echo ---
tbin/test_send_back.x
echo ---
tbin/test_forward.x
echo ---
tbin/test_signals.x
echo ---
tbin/test_bomb.x
