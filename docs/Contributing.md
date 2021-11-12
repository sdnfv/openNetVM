# Contribution Guidelines

To contribute to OpenNetVM, please follow these steps:

  1. Please read our [style guide][style]
  2. Create your own fork of the OpenNetVM repository
  3. Add our master repository as an upstream remote:
      - `git remote add upstream https://github.com/sdnfv/openNetVM`
  4. Update the `develop` branch before starting your work:
      - `git pull upstream develop`
  5. Create a branch off of `develop` for your feature.
      - We follow the fork/branch workflow where no commits are ever made to `develop` or `master`.  Instead, all development occurs on a separate feature branch.  Please read [this guide][gitflow] on the Git workflow.
  6. Add your commits
      - Good commit messages contain both a subject and body.  The subject provides an overview whereas the body answers the _what_ and _why_.  The body is usually followed by a change list that explains the _how_, and the commit ends with a _test plan_ describing how the developer verified their change.  Please read [this guide][commitguide] for more information.
  7. When you're ready to submit a pull request, rebase against `develop` and clean up any merge conflicts
      - `git pull --rebase upstream develop`
  8. Please fill out the pull request template as best as possible and be very detailed/thorough.

[style]: ../style/styleguide.md
[gitflow]: https://guides.github.com/introduction/flow/
[commitguide]: https://chris.beams.io/posts/git-commit/
