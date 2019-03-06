### Review checklist:  

- Sanity checks, assigned to @
    - Run make in `/onvm` and `/examples` directories
    - Test new functionality or bug fix from the pr (if needed)

- Code style, assigned to @
    - Run linter
    - Check everything style related

- Code design, assigned to @
    - Check for memory leaks
    - Check that code is reusable
    - Code is clean, functions are concise
    - Verify that edge cases properly handled

- Performance, assigned to @
    - Run Speed Tester NF, report performance.
    - Run pktgen, report performance (if needed)
    - Run mTCP epserver, verify wget works, report performance for epwget/ab (if needed)

- Documentation, assigned to @
    - Check if the new changes are well documented, in both code and READMEs
