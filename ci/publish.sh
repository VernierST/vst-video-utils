#!/bin/bash

echo "Pushing changes to GitHub"

echo "Deleting previous tags"
#git tag -l | xargs -n 1 git push --delete origin
#git tag | xargs git tag -d

echo "//registry.npmjs.org/:_authToken=$NPM_TOKEN" > ~/repo/.npmrc

npm install

curl -o- https://raw.githubusercontent.com/creationix/nvm/v0.34.0/install.sh | bash

export NVM_DIR="/opt/circleci/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"
nvm install 10.0.0

git pull 

echo "Publishing" $1 
npm run release -- $1 --ci --no-git.requireCleanWorkingDir
