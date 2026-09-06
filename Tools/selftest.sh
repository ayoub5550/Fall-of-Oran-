#!/bin/bash
cd /work/repos/fall-of-oran-ue5
for L in 0 1 2; do
LP_NUM_THREADS=1 timeout 150 /work/repos/unrealengine/Engine/Binaries/Linux/UnrealEditor-Cmd $PWD/FallOfOran.uproject -game -nullrhi -nosound -unattended -log -FOLevel=$L -FOSelfTest > /work/temp/dbg/selftest_L$L.log 2>&1
echo "EXIT=$?" >> /work/temp/dbg/selftest_L$L.log
pkill -9 -f "UnrealEditor-Cm[d]" 2>/dev/null
done
echo ALLDONE > /work/temp/dbg/selftest_done
