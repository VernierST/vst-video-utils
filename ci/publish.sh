#!/bin/bash

echo "Pushing changes to GitHub"


echo "Deleting previous tags"
#git tag -l | xargs -n 1 git push --delete origin
#git tag | xargs git tag -d

npm install

nvm install 10.0.0
#git checkout -b circle-publish-branch
#git push --set-upstream origin circle-publish-branch

echo "Publishing" $1 
npm run release -- $1 --ci --no-git.requireCleanWorkingDir

#git add .

#git commit -m Committing $1 changes to repo
#git push

echo "Branch 'circle-publish-branch' created in the GitHub repo.  Please merge with master"
