# README

Assignment A2 in the course Computer Systems @ DIKU, UCPH

Asger Ussing - ctz435  
Sebastian Andreassen - bwj836  
Victor Panduro Andersen - xkf701

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

**To run the programs with coverage:**

Follow step 1-3 from above.

To run any of the programs with valgrind memory leak coverage, run the command:

~~~bash
valgrind -s ./<program and its parameters> 
~~~

To run any of the programs with helgrind thread safety coverage, run the command:

~~~bash
valgrind --tool=helgrind -s ./<program and its parameters>
~~~

To benchmark and test the programs' running times, run the command:

~~~bash
time ./<program and its parameters>
~~~
