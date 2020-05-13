#!/bin/bash

# Don't output info for old (deleted) or unchanged (context) lines
# For new and changed lines, output one LINE_RANGE per line
diff --old-group-format="" --unchanged-group-format="" \
     --new-group-format="%dF-%dL%c'\012'" --changed-group-format="%dF-%dL%c'\012'" \
     "$1" "$2" | while IFS=- read -r LINE END; do
         echo -n "$LINE"
	
	 # If there's multiple lines, set a comma beforehand
         if [[ ! $LINE == "$END" ]]
         then
             echo -n " $END"
         fi
         echo
done 
