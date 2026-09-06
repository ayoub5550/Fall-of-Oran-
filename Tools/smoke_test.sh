#!/bin/bash
# usage: smoke.sh <levelIndex>  -> /work/temp/dbg/smoke_L<idx>.log
L=$1
cd /work/repos/fall-of-oran-ue5
LP_NUM_THREADS=1 timeout 150 /work/repos/unrealengine/Engine/Binaries/Linux/UnrealEditor-Cmd $PWD/FallOfOran.uproject -game -nullrhi -nosound -unattended -log -FOLevel=$L -FOShots -FOShotMax=2 > /work/temp/dbg/smoke_L$L.log 2>&1
echo "EXIT=$?" >> /work/temp/dbg/smoke_L$L.log
pkill -9 -f "UnrealEditor-Cm[d]" 2>/dev/null
