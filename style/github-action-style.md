# A CI Static Analysis tool.

This CI is supported by Github Actions and is triggered when users make pull request to openNetVM.
It's main purpose is to give warnings and suggestions for C, Shell and python scripts. 

Some of the goals of the Linter include:

* Give feedback to the user's that may have made pull requests to an incorrect branch.

* Identify places in user's code that *may* not be in non-compliance with [ONVM style rules.](styleguide.md)

* It does not attempt to fix up these problems but educate.

## Table of Contents

* [How it works](#how-it-works)
* [Installing](#installing)
* [How to use](#how-to-use)
* [Github Actions](#github-actions)
* [Other Resources](#other-resources)

### How it works.

The Linter can be broken down into a few simple steps.

* Github Action triggers *run-lint.sh* script.
* Depending on the argument passed in, lint using one of the following static analysis tools: \
  **shellcheck, cppcheck, gwclint.py, pylint**
* Get the name of the files the user has made changes to.
* Get the lines that the user has modified.
* Perform lint on the files using the corresponding linter specified from step two. 
* Create a list of errors from the linter's output that only includes lines that has been written/modified by the user.

## Installing

* The following three linters need to be installed before you are able to run the linter in your terminal. \
**pylint 2.4.4** \
**Cppcheck 1.90-4build1** \
**ShellCheck 0.7.0**

Run **./style/install-linters.sh** to install the different static tools.

If you encounter errors during installation, you *may* install the latest version that your OS supports with **pip3 install pylint** and **sudo apt-get install cppcheck**

* [Other Resources](#other-resources)

Although the correct linter versions *may* be installed without versions being specified, it's a good idea to manually install the specific versions regardless.
This is especially so as these commands are run every time a new workflow has started. This avoids any surprise build breaks when a new version with new warnings or output formatting is published.
  
## How to use

### From your terminal

To test the files you would like to be linted, you may run one of the following commands.

**style/run-lint.sh cppcheck** -- Lints C files with Cppcheck. \
**style/run-lint.sh c** -- Lints C files for styling errors with gwclint.py. \
**style/run-lint.sh python** -- Lints Python files with pylint. \
**style/run-lint.sh shell** -- Lints bash shell script files with shellcheck.

### Pull requests.

All four linters are automatically triggered when a pull request has been made. The workflow is synchronized with the current pr and gets automatically restarted if someone has made a push to the current open PR.
Therefore, it is highly suggested to [turn off email notification for githubAction runs.](https://help.github.com/en/github/receiving-notifications-about-activity-on-github/about-email-notifications)

## Github Actions

The **.github/workflows/Linter.yml** script is triggered when pull request are opened. The workflow is automatically re-run when a pull request is made. \
The workflow has five separate jobs that run concurrently.

* **Branch** -- Checks if the user has made PR to the correct branch. If not, comment on pr to suggest change.
* **Cppcheck** -- Lints C files with Cppcheck. 
* **C** -- Lints C files for styling errors with gwclint.py 
* **Python** -- Lints Python files with pylint. 
* **Shell** -- Lints bash shell script files with shellcheck


## Other Resources

To learn more about how the linters function you may visit one of the following resources.
* https://github.com/koalaman/shellcheck
* https://www.pylint.org/
* http://cppcheck.sourceforge.net/

If you would like to learn more about Github Actions:
* https://help.github.com/en/actions/configuring-and-managing-workflows
