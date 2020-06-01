#!/usr/bin/python3
'''
Run designated linter on files from git diff
Only output errors that were changed in this branch --> upstream/develop
'''
import os
import sys

def git_changed_files():
    '''
    Run system command to get all C files modified.
    Return in an array split by line.
    '''
    git_files_cmd = 'git diff --name-only upstream/develop {0}' .format(sys.argv[2])
    return os.popen(git_files_cmd).read().splitlines()

def git_line_changes(file):
    '''
    Run git command to get line changes.
    Returns array of strings.
    '''

    git_diff_cmd = 'git difftool --no-prompt --extcmd "./style/git-difftool-changed-lines.sh" \
                -U0 upstream/develop -- '
    return os.popen(git_diff_cmd + file).read().splitlines()

def gwlint_output(file_name):
    '''
    Return output from gwclint and separate into lines for an array
    '''
    # Run the specified linter with the input filename.
    # Only output lines that contain the specfied filename.
    get_python_lint_cmd = '{0} {1} 2>&1\
            | grep -E {1} '.format(sys.argv[1], file_name)
    lint_lines = os.popen(get_python_lint_cmd).read().splitlines()

    # If lint with gwclint remove the last line of lint: "Done processing example-filename.c"
    if sys.argv[1] == "python ./style/gwclint.py --verbose=4":
        lint_lines = lint_lines[:-1]
    # If linting with cppcheck, we only want errors. Remove lines such as Checking..."
    elif sys.argv[1] == "cppcheck":
        cpp_lint_lines = []
        for line in lint_lines:
            if "error" in line:
                cpp_lint_lines.append(line)
        return cpp_lint_lines
    return lint_lines

def get_lint_from_file(lint_file, line_inx):
    '''
    Line has to be parsed to find the file line number.
    '''
    line = lint_file[line_inx]
    number = line.split(":", 2)[1]

    # Cppcheck return return lints in the format [filename.c:488].
    # Remove trailing ']' and set to line number.
    if sys.argv[1] == "cppcheck":
        number = number[:-1]
    # Return both the file line number, and the actual line linted.

    return number, line

def get_start_end_from_diff(git_diff, line_inx):
    '''
    Lines are either in the form of
    <start_line>
    or
    <start line> <end ine>
    Returns a parsed line
    '''

    diffs = git_diff[line_inx].split()

    # Start and end returned (can be the same).
    return diffs[0], diffs[-1]

def log_errors(logfile, all_errors):
    '''
    Output list of errors to file line by line.
    '''

    with open(logfile, "w") as linting_out:

        for error in reversed(all_errors):
            linting_out.write(error + "\n")

        linting_out.write("Total errors found: {}\n".format(len(all_errors)))

def run_linter(lint_outfile):
    '''
    Run linter only on changed lines, and put errors into lint_outfile.
    '''

    # Grab the files from git diff.
    files = git_changed_files()
    error_list = []
    # Loop through each changed file.
    for modified_file in files:
        start = end = error_line = line = None
        # Initialize git line change variables.
        git_pr_differences = git_line_changes(modified_file)
        # Start at last git change.
        diff_inx = len(git_pr_differences) - 1

        # Initialize linter error variables.
        file_lint = gwlint_output(modified_file)
        # Start at last error in the list.
        lint_inx = len(file_lint) - 1

        # Loop through git diff and linter simultaneously.
        # Moves through the lists reversed to potentially end quicker.
        while diff_inx >= 0 and lint_inx >= 0:
            # Update the indices in the list.
            start, end = get_start_end_from_diff(git_pr_differences, diff_inx)
            error_line, line = get_lint_from_file(file_lint, lint_inx)

            if int(start) <= int(error_line) <= int(end):
                # Found another error, error_line was in range.
                error_list.append(line)
                # Move to next in the lint output.
                lint_inx -= 1
            elif int(error_line) > int(end):
                # Move to next from linter.
                lint_inx -= 1

            elif int(error_line) < int(start):
                # Move to next git change.
                diff_inx -= 1
    if error_list:
        # Only output to file if we had errors.
        log_errors(lint_outfile, error_list)

if __name__ == "__main__":
    run_linter("linter_out.txt")
