git status
echo "========= START Listing Differences ========================"
git status | awk '$1 == "modified:" { print ($2)}' | xargs git add
git status | awk '$1 == "deleted:" { print ($2)}' | xargs git rm
echo "========= END Listing Differences ========================"
