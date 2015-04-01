# CloudLab Coding Style Guide

As a student, you have spent years writing code that was read by at most two or three people. This has allowed you to get away with terrible coding habits. In the real world, writing good code is **important**, but writing readable code is **critical**.

The goal of this guide is to give a concise set of guidelines, with references to other documents that have broader coverage and more justification where needed.  It will take some time to change your habits to write better structure code, but it is worth the thought and effort.

This document assumes code is being written in C, but most of the rules can be applied more generally to other languages.

## File and Folder Structure

Your project is likely to include multiple files, so you need to think about how to name them and where to store them. Common sense should be a sufficient guide for most of this.

**File Names** should be short but specific to the module residing within them. Reading a list of file names should give someone a quick idea of the main components in a project.

**A Header (.h) File** should generally be included for each .c file.  The header should be self-contained and include a `#ifdef` guard to prevent compiler errors. [More Details.](http://google-styleguide.googlecode.com/svn/trunk/cppguide.html#Header_Files)

**Folders** should be used to separate out major components in large projects. For smaller projects this is not necessary.

**Makefiles** should be included with every project. This gives a reader an entry point to understand the main files in your project and how to execute it. If it takes more than `make` to build the project, or if there are multiple important targets, the Makefile should explain.

**A readme.md file** should be included with every project. Learn to use [Markdown to format your documents](https://guides.github.com/features/mastering-markdown/).

## Variable and Function Naming
How you name your variables, data structures, and functions can be the dominant factor on whether your code is readable. In general, we follow the [Composite](https://github.com/gparmer/Composite/blob/master/doc/style_guide/composite_coding_style.pdf?raw=true) and [Google](http://google-styleguide.googlecode.com/svn/trunk/cppguide.html#Naming) naming conventions, although Google's recommendations are for C++ and thus use more capital letters than you are likely to use in a C program.

### Understandable, Lower Case with Underscores
Variable names should be meaningful and help users understand what they are for. Do not use CamelCase unless you are writing Java programs. In C, we prefer all lowercase function and variable names, with underscores between words where necessary. Avoid abbreviations except for very obvious cases.

  * **Good:** `num_users, read_socket,   check_param`
  * **Bad:** `n, readsocket, readSocket, check_prm`

### Single Letter Iterators
You can use ``i, j, k`` for iterators in loops. Otherwise, avoid single letter variable names.

## Code Structure
Names are important, but the overall structure of your functions and files also makes a difference.

### Variable Declaration Positioning
Variables should be declared at the top of code blocks. The [Composite Style Guide](https://github.com/gparmer/Composite/blob/master/doc/style_guide/composite_coding_style.pdf?raw=true) gives some justification.

### Small, Readable Functions
Functions should be short, single purpose chunks of code that follow the Onion Model--a reader should be able to look at the outermost function and quickly understand how it works before diving into the actual algorithmic details that will reside in other functions.

This example is stolen from the [Composite Style Guide](https://github.com/gparmer/Composite/blob/master/doc/style_guide/composite_coding_style.pdf?raw=true):
````
int request_handle(char *request_txt)
{
        struct request req;
        struct response resp;
        request_parse(&req, request_txt);
        request_process(&req);
        request_formulate_resp(&req, &resp);
        request_send_resp(&req, &resp);
}
````

This function does nothing but call other functions. This may feel like a waste or inefficient, but it is infinitely more readable than if all the code for all of those functions had been combined together here. The compiler will handle perfromance--your job is to write readable code.

### Error Cases and Exception Handling
We follow the [Composite Style Guide's](https://github.com/gparmer/Composite/blob/master/doc/style_guide/composite_coding_style.pdf?raw=true) suggestions for structuring how conditionals affect the flow of your code.

**Convention 1.** Make sure that the "common-case" code path through your functions reads from top to bottom, and at the same indentation level as much as possible. To achieve this, make your conditionals be true on error.

**Convention 2.** Use goto statements to avoid code redundancy and handle error cases. These can and should be used as exceptions that are local to the function (without all the cruft of real exceptions).

**Convention 3.** Do error handling as soon as you can in a function. This is most important for input (function argument) sanity checking.

Read the Composite guide for examples and justification.

## Formatting
We follow the [Composite Style Guide's](https://github.com/gparmer/Composite/blob/master/doc/style_guide/composite_coding_style.pdf?raw=true) formatting recommendations.

**Blank lines are used to separate "thoughts".** Leave an empty line between variable declaration blocks and code, or between chunks of code that have a different purpose. Think of it as the separator between paragraphs.

**Curly brace positioning** depends on if it is for a function or a conditional/loop. Place braces for functions on a new line because it helps visually separate it. Braces for conditionals/loops are required and should be kept on the same line as the condition. For example:

````
1: int some_func(int x)
2: {
3:         if(x < 0){
4:                 printf("X is negative.");
5:         }
6: }
````
Here the conditional's body is placed within braces even though it is a single line, and the brace is kept on line 3. The function name's brace is put on line 2 to help make the function stand out.


**Use 8 spaces to define a tab.** This makes tabs wide, discouraging you from writing code that is deeply nested. It also matches the Linux standard. Learn how to setup your editor to do this for you.

## Makefiles
Please follow the guidelines below when creating makefiles.

- Include targets for `make all` and `make clean` (`make rebuild` can be useful for enforcing recompilation)
	* `make all` should compile and create executables for all runnable files
	* `make clean` should remove all executables created by `make all` and should include the *-f* flag
- Variables can be defined simply by putting `{Variable_Name} = {Variable_Value}` and are referenced by using `$({Variable_Name})`
	* EX: `CC = gcc` - this defines our compiler and can be called later by `$(CC)`
- Each executable file should have a target and compilation method defined in the makefile

**Makefile cheatsheet:**

* target - refers to the name of an action or file to be created
* `$@` - variable reference to the target
* `$^` - variable representing all the prerequisites (C files) listed for the given target (no repititions)
* `$<` - variable representing the first prerequisite of the target

**Sample Makefile:**
````
#Use the gcc compiler
CC = gcc
CFLAGS = 
DEPS = 
LDFLAGS = 
#Objects created by makefile
OBJS = test-file

#Ensure compiling is done with all necessary dependencies
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

all: $(OBJS)

rebuild: clean all

#Builds test-file
test-file: test-file.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	@rm -f $(OBJS)
````

## Git Usage
We use git to track code changes. Learn how to use it.

**Commit messages** should be at least one sentence and should help a reader understand what you have modified.

**Use a .gitignore** file in every project. This specifies what files git will ignore. Learn how this works and find a default file for your language of choice as a starting point.

**Add files carefully.** Your repository should only include the source code and documentation needed by the project (not temporary files or executables). You should almost never add an entire directory to a commit. Doing so is likely to add a large number of undesirable files, and it is very difficult to remove them later. If you aren't sure whether a command will add unnecessary files, use `git add -n` to do a "dry run" of the specified command.

**Forks.** When there are multiple students working on a project, we generally have each student create a fork of the base repository into their own account and work within that. This allows you to have a messy version of the repository where some commits break portions of the code, without affecting the main repository. However, we still generally use the Issue tracker of the main repository as the central place to track what needs to be done.

**Branches.** We tend to do everything in the `master` branch, but consider creating a separate branch if you are working on a risky feature that may need to be rolled back, or if multiple students are working within the same repository. Branches are also an excellent way to make small temporary changes to code, for example when running slightly different experiments.

**Pushing and Pulling.** You should regularly push your code to your fork of the repository. Once you have made a set of cohesive (and working) changes, then you should create a Pull Request to have it merged with the base repository. Keep in mind that others may be changing the base repository, so you will have to both push your code to the base and pull from the base to your version. Learn how to create a remote named `upstream` in your git repo that represents the base version of the repository (i.e., not the version forked into your account).

**Notifications.** You should setup github's notification system so that it will email you when changes are made to repositories that you are working on. For example, if someone tags you in a commit message or assigns an Issue to you, then you should be sure you will be notified of this.  It's a good idea to regularly look at your github web page to see what updates other students are making on projects relevant to you.

## More Resources
The contents of this guide are largely stolen from the [Composite Style Guide](https://github.com/gparmer/Composite/blob/master/doc/style_guide/composite_coding_style.pdf?raw=true). Where not discussed here, follow the Composite rules.

[Google's C++ Style Guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.html#Header_Files) also provides many reasonable style choices, and is used by many different companies. Where not discussed here or in the Composite guide, follow Google's.
