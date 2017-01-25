openNetVM Web Display
==

About
--

Resources to view manager statistics through a web browser

Usage
--

Once this has been installed, point a web browser to
`http://<hostname>/onvm_web`

Installation
--

1. In `openNetVM/onvm_web/setup/` run the file `setup_web.sh`.  This
   scripts installs all dependencies and sets up Apache2 to point to the
openNetVM directory.  During its execution, you will be notified to edit
two files, and then the files will open inside VIM.  Find the comments
marked with `TODO:` and do as they say.
   
     `sudo ./openNetVM/onvm_web/setup/setup_web.sh`
