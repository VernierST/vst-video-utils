NPMVERSION="$(npm view @vernier/vst-video-utils-build version)"
PACKAGEVERSION="$(grep "version" package.json | awk '{print $2}' | cut -d\" -f2)"

echo $NPMVERSION is latest version on NPM
echo $PACKAGEVERSION is version number in package.json

if [ "$NPMVERSION" = "$PACKAGEVERSION" ]; then
    PUBLISH=false
else
    PUBLISH=true
fi

if [ "$PUBLISH" == true ]; then
   echo PUBLISHING
   sh ./ci/ciScripts/publishPackage.sh
fi