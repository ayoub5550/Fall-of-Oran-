#!/bin/bash
export JAVA_HOME=/work/android/jdk
export ANDROID_HOME=/work/android/sdk ANDROID_SDK_ROOT=/work/android/sdk
export NDKROOT=/work/android/sdk/ndk/27.2.12479018 ANDROID_NDK_ROOT=/work/android/sdk/ndk/27.2.12479018
export PATH=/work/temp/fakebin:/work/android/jdk/bin:$PATH
cd /work/repos/unrealengine
bash Engine/Build/BatchFiles/RunUAT.sh BuildCookRun -project=/work/repos/fall-of-oran-ue5/FallOfOran.uproject -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -package -pak -archive -archivedirectory=/work/temp/apk_v17 -nop4 -utf8output -unattended -NoUBA -NoUBALocal -ddc=NoZenLocalFallback > /work/logs/package_android_v17.log 2>&1
echo "EXIT=$?" >> /work/logs/package_android_v17.log
