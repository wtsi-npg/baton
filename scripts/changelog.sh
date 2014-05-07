#!/bin/sh

label=Current

git tag | sort -u -r | while read tag ; do
    echo
    echo
    echo [$label]
    echo
    git --no-pager log --no-merges $tag..$prev
    prev=$tag
    label=$prev
done

tag=$(git tag | head -1)
echo
echo
echo [$tag]
echo
git --no-pager log --no-merges $tag
