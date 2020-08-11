.. Contribution guide

Contribution Guidelines
=====================================

To contribute to OpenNetVM, please follow these steps:

1. Please read our style guide (TODO: MAKE LINK)

2. Create your own fork of the OpenNetVM repository

3. Add our master repository as an upstream remote:

.. code-block:: bash
    :linenos:
    
    git remote add upstream https://github.com/sdnfv/openNetVM

4. Update the :code:`develop` branch before starting your work:

.. code-block:: bash
    :linenos:
    
    git pull upstream develop

5. Create a branch off of :code:`develop` for your feature.

We follow the fork/branch workflow where no commits are ever made to :code:`develop` or :code:`master`.  Instead, all development occurs on a separate feature branch. Please read `this guide <https://guides.github.com/introduction/flow/>`_ on the Git workflow.
      
6. When contributing to documentation for ONVM, please see this `Restructured Text (reST) guide <https://thomas-cokelaer.info/tutorials/sphinx/rest_syntax.html>`_ for formatting.

7. Add your commits

Good commit messages contain both a subject and body.  The subject provides an overview whereas the body answers the *what* and *why*.  The body is usually followed by a change list that explains the *how*, and the commit ends with a *test plan* describing how the developer verified their change.  Please read `this guide <https://chris.beams.io/posts/git-commit/>`_ for more information.

8. When you're ready to submit a pull request, rebase against :code:`develop` and clean up any merge conflicts

.. code-block:: bash
    :linenos:
   
    git pull --rebase upstream develop
   
9. Please fill out the pull request template as best as possible and be very detailed/thorough.

