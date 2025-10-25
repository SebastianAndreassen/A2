# README

Assignment A2 in the course Computer Systems @ DIKU, UCPH

Asger Ussing - ctz435 <br> Sebastian Andreassen - bwj836 <br> Victor Panduro Andersen - xkf701

<https://github.com/SebastianAndreassen/A2>

---

**To run any of the programs:**

1. Download the full repository to your client.
2. Navigate to the src folder in your terminal.
3. Run the command:

    ~~~bash
    make all
    ~~~

4. To run the multithreaded fauxgrep program with n-number of threads, run the command:

    ~~~bash
    ./fauxgrep-mt -n <number of threads> <substring to search for> <file or directory to search in>
    ~~~

    To run the multithreaded fhistogram program with n-number of threads, run the command:

    ~~~bash
    ./fhistogram-mt -n <number of threads> <file or directory to search in>
    ~~~

---

**To run the test script:**

1. Follow step 1-3 from above.
2. Run the command:

    ~~~bash
    ./tests.sh
    ~~~

3. Locate and view the "valgrind_output.txt" and "helgrind_output.txt" files in the src/test_results folder.

If you do not have the permission to run the test script, then run the command:

~~~bash
chmod +x tests.sh
~~~

And then rerun 2.
